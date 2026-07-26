#pragma once
#include "TimemoreFramedScale.h"
#include "TimemoreUtil.h"

class TimemoreDotScale : public TimemoreFramedScale {
 public:
  void onConnected() override { sendInitSequence(); }

  static bool matches(const std::string& deviceName) {
    const std::string name = timemoreToLower(deviceName);
    return name.find("dot") != std::string::npos || name.find("tes017") != std::string::npos;
  }

 protected:
  bool validateIncomingCrc() const override { return false; }
};
