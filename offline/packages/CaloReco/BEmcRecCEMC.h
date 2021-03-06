#ifndef CALORECO_BEMCRECCEMC_H
#define CALORECO_BEMCRECCEMC_H

#include "BEmcRec.h"

#include <string>
#include <vector>

class BEmcProfile;
class EmcModule;

class BEmcRecCEMC : public BEmcRec
{
 public:
  BEmcRecCEMC();
  virtual ~BEmcRecCEMC();
  void CorrectEnergy(float energy, float x, float y, float *ecorr) override;
  void CorrectECore(float ecore, float x, float y, float *ecorecorr) override;
  void CorrectPosition(float energy, float x, float y, float &xcorr, float &ycorr) override;
  void CorrectShowerDepth(float energy, float x, float y, float z, float &xc, float &yc, float &zc) override;

  void LoadProfile(const std::string &fname) override;
  float GetProb(std::vector<EmcModule> HitList, float e, float xg, float yg, float zg, float &chi2, int &ndf) override;

 private:
  BEmcProfile *_emcprof;
};

#endif
