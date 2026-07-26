#pragma once
#include "TimemoreScale.h"
#include "TimemoreUtil.h"

// The original "Timemore Scale" (classic/Nano generation), which exposes
// the standard Bluetooth SIG Weight Scale service (0x181D) instead of a
// proprietary one, and reports two independent weight channels.
class TimemoreClassicScale : public TimemoreScale {
 public:
  const char* serviceUuid() const override { return "0000181d-0000-1000-8000-00805f9b34fb"; }
  const char* notifyCharUuid() const override { return "00002a9d-0000-1000-8000-00805f9b34fb"; }
  const char* commandCharUuid() const override { return "553f4e49-bf21-4468-9c6c-0e4fb5b17697"; }
  bool writeWithoutResponse() const override { return false; }

  void tare() override {
    setWeight(0.0f, 0.0f);
    const uint8_t cmd = 0x00;
    writeRaw(&cmd, 1);
  }

  void setTimer(TimemoreTimerCommand command) override {
    uint8_t cmd = 0;
    switch (command) {
      case TimemoreTimerCommand::START:
        cmd = 0x08;
        break;
      case TimemoreTimerCommand::STOP:
        cmd = 0x09;
        break;
      case TimemoreTimerCommand::RESET:
        cmd = 0x0A;
        break;
    }
    writeRaw(&cmd, 1);
  }

  void handleNotification(const uint8_t* data, size_t len) override {
    if (len < 5) return;

    const bool isDoubleScale = (data[0] & 0x10) != 0;
    const int32_t totalRaw = readInt32LE(data + 1);

    int32_t downRaw = totalRaw;
    if (isDoubleScale && len >= 9) {
      downRaw = readInt32LE(data + 5);
    }

    const float total = static_cast<float>(totalRaw) / 10.0f;
    const float down = static_cast<float>(downRaw) / 10.0f;
    setWeight(total, down);
  }

  static bool matches(const std::string& deviceName) {
    const std::string name = timemoreToLower(deviceName);
    return name.find("timemore scale") != std::string::npos ||
           name.find("black mirror") != std::string::npos;
  }

 private:
  static int32_t readInt32LE(const uint8_t* p) {
    return static_cast<int32_t>(
        static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24));
  }
};
