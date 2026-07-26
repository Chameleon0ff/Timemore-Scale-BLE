#pragma once
#include "TimemoreProtocol.h"
#include "TimemoreScale.h"

// Shared implementation for the "framed" Timemore protocol: service FFF0,
// notify char FFF1, command char FFF2, frames shaped
// [0xA5 0x5A][opcode][cmdId][len:2][data][crc16:2]. DOT and Basic3 only
// differ in whether they validate the incoming CRC and how their name is
// matched - see TimemoreDotScale.h / TimemoreBasicScale.h.
class TimemoreFramedScale : public TimemoreScale {
 public:
  const char* serviceUuid() const override { return "0000fff0-0000-1000-8000-00805f9b34fb"; }
  const char* notifyCharUuid() const override { return "0000fff1-0000-1000-8000-00805f9b34fb"; }
  const char* commandCharUuid() const override { return "0000fff2-0000-1000-8000-00805f9b34fb"; }
  bool writeWithoutResponse() const override { return true; }

  void tare() override {
    setWeight(0.0f);
    uint8_t frame[8];
    const size_t len = TimemoreProtocol::buildFrame(0x03, 0x0D, nullptr, 0, frame, sizeof(frame));
    writeRaw(frame, len);
  }

  void setTimer(TimemoreTimerCommand command) override {
    uint8_t payload = 0;
    switch (command) {
      case TimemoreTimerCommand::START:
        payload = 0x01;
        break;
      case TimemoreTimerCommand::STOP:
        payload = 0x02;
        break;
      case TimemoreTimerCommand::RESET:
        payload = 0x03;
        break;
    }
    uint8_t frame[9];
    const size_t len = TimemoreProtocol::buildFrame(0x03, 0x02, &payload, 1, frame, sizeof(frame));
    writeRaw(frame, len);
  }

  bool requestBattery() override {
    uint8_t frame[8];
    const size_t len = TimemoreProtocol::buildFrame(0x02, 0x05, nullptr, 0, frame, sizeof(frame));
    if (len == 0) {
      return false;
    }
    writeRaw(frame, len);
    return true;
  }

  void handleNotification(const uint8_t* data, size_t len) override {
    size_t pos = 0;
    while (pos + 8 <= len) {
      if (data[pos] != 0xA5 || data[pos + 1] != 0x5A) {
        pos++;
        continue;
      }

      const TimemoreProtocol::ParsedFrame frame =
          TimemoreProtocol::tryParseFrame(data + pos, len - pos, validateIncomingCrc());
      if (!frame.valid) {
        pos++;
        continue;
      }

      if (frame.opcode == 0x01 || frame.opcode == 0x02) {
        switch (frame.cmdId) {
          case 0x01:  // weight / flow rate / time
            if (frame.dataLen >= 8) {
              const int32_t raw = static_cast<int32_t>(
                  (static_cast<uint32_t>(frame.data[0]) << 24) |
                  (static_cast<uint32_t>(frame.data[1]) << 16) |
                  (static_cast<uint32_t>(frame.data[2]) << 8) | frame.data[3]);
              setWeight(static_cast<float>(raw) / 10.0f);
            }
            break;
          case 0x05:  // battery level + percent
            if (frame.dataLen >= 2) {
              const uint8_t b0 = frame.data[0];
              const uint8_t b1 = frame.data[1];
              if (b1 <= 100) {
                setBattery(b1);
              } else if (b0 <= 100) {
                setBattery(b0);
              }
            } else if (frame.dataLen >= 1) {
              if (frame.data[0] <= 100) {
                setBattery(frame.data[0]);
              }
            }
            break;
        }
      }

      pos += 8 + frame.dataLen;
    }
  }

 protected:
  virtual bool validateIncomingCrc() const = 0;

  void sendInitSequence() {
    delay(500);

    uint8_t unitPayload = 0x00;  // unit: gram
    uint8_t unitFrame[9];
    const size_t unitLen =
        TimemoreProtocol::buildFrame(0x03, 0x06, &unitPayload, 1, unitFrame, sizeof(unitFrame));
    writeRaw(unitFrame, unitLen);

    delay(200);

    uint8_t modePayload[2] = {0x01, 0x00};  // mode: standard
    uint8_t modeFrame[10];
    const size_t modeLen =
        TimemoreProtocol::buildFrame(0x03, 0x08, modePayload, 2, modeFrame, sizeof(modeFrame));
    writeRaw(modeFrame, modeLen);

    delay(100);
    requestBattery();
  }
};
