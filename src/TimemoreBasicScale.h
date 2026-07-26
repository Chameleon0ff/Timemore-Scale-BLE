#pragma once
#include "TimemoreFramedScale.h"
#include "TimemoreUtil.h"

class TimemoreBasicScale : public TimemoreFramedScale {
 public:
  void onConnected() override { sendInitSequence(); }

  static bool matches(const std::string& deviceName) {
    const std::string name = timemoreToLower(deviceName);
    return name.find("basic3") != std::string::npos || name.find("basic 3") != std::string::npos ||
           (name.find("timemore") != std::string::npos && name.find("basic") != std::string::npos);
  }

 protected:
  bool validateIncomingCrc() const override { return true; }
};
