// This is the new trackbase container version

#include "PHG4MvtxDigitizer.h"

// Move to new storage containers
#include <trackbase/TrkrDefs.h>
#include <trackbase/TrkrHit.h>                      // for TrkrHit
#include <trackbase/TrkrHitSet.h>
#include <trackbase/TrkrHitSetContainer.h>

#include <g4detectors/PHG4CylinderGeom.h>
#include <g4detectors/PHG4CylinderGeomContainer.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/SubsysReco.h>                     // for SubsysReco

#include <phool/PHCompositeNode.h>
#include <phool/PHNode.h>                           // for PHNode
#include <phool/PHNodeIterator.h>
#include <phool/PHRandomSeed.h>
#include <phool/getClass.h>
#include <phool/phool.h>                            // for PHWHERE

#include <gsl/gsl_rng.h>                            // for gsl_rng_alloc

#include <cstdlib>                                 // for exit
#include <iostream>
#include <set>

using namespace std;

PHG4MvtxDigitizer::PHG4MvtxDigitizer(const string &name)
  : SubsysReco(name)
  , _energy_threshold(0.95e-6)
{
  unsigned int seed = PHRandomSeed();  // fixed seed is handled in this funtcion
  cout << Name() << " random seed: " << seed << endl;
  RandomGenerator = gsl_rng_alloc(gsl_rng_mt19937);
  gsl_rng_set(RandomGenerator, seed);

  if (Verbosity() > 0)
    cout << "Creating PHG4MvtxDigitizer with name = " << name << endl;
}

PHG4MvtxDigitizer::~PHG4MvtxDigitizer()
{
  gsl_rng_free(RandomGenerator);
}

int PHG4MvtxDigitizer::InitRun(PHCompositeNode *topNode)
{
  //-------------
  // Add Hit Node
  //-------------
  PHNodeIterator iter(topNode);

  // Looking for the DST node
  PHCompositeNode *dstNode = dynamic_cast<PHCompositeNode *>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
  {
    cout << PHWHERE << "DST Node missing, doing nothing." << endl;
    return Fun4AllReturnCodes::ABORTRUN;
  }

  CalculateMvtxLadderCellADCScale(topNode);

  //----------------
  // Report Settings
  //----------------

  if (Verbosity() > 0)
  {
    cout << "====================== PHG4MvtxDigitizer::InitRun() =====================" << endl;
    for (std::map<int, unsigned int>::iterator iter = _max_adc.begin();
         iter != _max_adc.end();
         ++iter)
    {
      cout << " Max ADC in Layer #" << iter->first << " = " << iter->second << endl;
    }
    for (std::map<int, float>::iterator iter = _energy_scale.begin();
         iter != _energy_scale.end();
         ++iter)
    {
      cout << " Energy per ADC in Layer #" << iter->first << " = " << 1.0e6 * iter->second << " keV" << endl;
    }
    cout << "===========================================================================" << endl;
  }

  return Fun4AllReturnCodes::EVENT_OK;
}

int PHG4MvtxDigitizer::process_event(PHCompositeNode *topNode)
{
  // This code now only does the Mvtx
  DigitizeMvtxLadderCells(topNode);

  return Fun4AllReturnCodes::EVENT_OK;
}

void PHG4MvtxDigitizer::CalculateMvtxLadderCellADCScale(PHCompositeNode *topNode)
{
  // defaults to 8-bit ADC, short-axis MIP placed at 1/4 dynamic range

  PHG4CylinderGeomContainer *geom_container = findNode::getClass<PHG4CylinderGeomContainer>(topNode, "CYLINDERGEOM_MVTX");

  if (!geom_container) return;

  if (Verbosity())
    cout << "Found CYLINDERGEOM_MVTX node" << endl;

  PHG4CylinderGeomContainer::ConstRange layerrange = geom_container->get_begin_end();
  for (PHG4CylinderGeomContainer::ConstIterator layeriter = layerrange.first;
       layeriter != layerrange.second;
       ++layeriter)
  {
    int layer = layeriter->second->get_layer();
    float thickness = (layeriter->second)->get_pixel_thickness();
    float pitch = (layeriter->second)->get_pixel_x();
    float length = (layeriter->second)->get_pixel_z();

    float minpath = pitch;
    if (length < minpath) minpath = length;
    if (thickness < minpath) minpath = thickness;
    float mip_e = 0.003876 * minpath;

    if (Verbosity())
      cout << "mip_e = " << mip_e << endl;

    if (_max_adc.find(layer) == _max_adc.end())
    {
      _max_adc[layer] = 255;
      _energy_scale[layer] = mip_e / 64;
    }
  }

  return;
}

void PHG4MvtxDigitizer::DigitizeMvtxLadderCells(PHCompositeNode *topNode)
{
  //----------
  // Get Nodes
  //----------

  // new containers
  //=============
  // Get the TrkrHitSetContainer node
  TrkrHitSetContainer *trkrhitsetcontainer = findNode::getClass<TrkrHitSetContainer>(topNode, "TRKR_HITSET");
  if (!trkrhitsetcontainer)
  {
    cout << "Could not locate TRKR_HITSET node, quit! " << endl;
    exit(1);
  }

  // Digitization

  // We want all hitsets for the Mvtx
  TrkrHitSetContainer::ConstRange hitset_range = trkrhitsetcontainer->getHitSets(TrkrDefs::TrkrId::mvtxId);
  for (TrkrHitSetContainer::ConstIterator hitset_iter = hitset_range.first;
       hitset_iter != hitset_range.second;
       ++hitset_iter)
  {
    // we have an itrator to one TrkrHitSet for the mvtx from the trkrHitSetContainer
    // get the hitset key so we can find the layer
    TrkrDefs::hitsetkey hitsetkey = hitset_iter->first;
    int layer = TrkrDefs::getLayer(hitsetkey);
    if (Verbosity() > 1) cout << "PHG4MvtxDigitizer: found hitset with key: " << hitsetkey << " in layer " << layer << endl;

    // get all of the hits from this hitset
    TrkrHitSet *hitset = hitset_iter->second;
    TrkrHitSet::ConstRange hit_range = hitset->getHits();
    std::set<TrkrDefs::hitkey> hits_rm;
    for (TrkrHitSet::ConstIterator hit_iter = hit_range.first;
         hit_iter != hit_range.second;
         ++hit_iter)
    {
      TrkrHit *hit = hit_iter->second;

      // Convert the signal value to an ADC value and write that to the hit
      unsigned int adc = hit->getEnergy() / _energy_scale[layer];
      if (adc > _max_adc[layer]) adc = _max_adc[layer];
      hit->setAdc(adc);

      // Remove the hits with energy under threshold
      bool rm_hit = false;
      if (hit->getEnergy() < _energy_threshold) rm_hit = true;
      if (Verbosity() > 0) cout << "    PHG4MvtxDigitizer: found hit with key: " << hit_iter->first << " and signal " << hit->getEnergy() << " and adc " << adc << " on layer " << layer << ", to remove hit " << rm_hit << endl;

      if (rm_hit) hits_rm.insert(hit_iter->first);
    }

    for (std::set<TrkrDefs::hitkey>::iterator hits_rm_iter = hits_rm.begin();
         hits_rm_iter != hits_rm.end();
         ++hits_rm_iter)
    {
      if (Verbosity() > 0) cout << "    PHG4MvtxDigitizer: remove hit with key: " << *hits_rm_iter << endl;
      hitset->removeHit(*hits_rm_iter);
    }

  }

  // end new containers
  //===============

  return;
}
