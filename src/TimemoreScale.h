#pragma once
#include <Arduino.h>
#include <functional>

enum class TimemoreTimerCommand : uint8_t { START, STOP, RESET };

// Base class shared by all Timemore models. TimemoreScaleManager wires up
// the actual BLE write function via _attachWriter() after subscribing to
// notifications - subclasses never touch NimBLE directly, they only build
// command bytes and parse notification bytes.
class TimemoreScale {
 public:
  using WeightCallback = std::function<void(float weightGrams, float weight2Grams)>;
  using BatteryCallback = std::function<void(uint8_t percent)>;
  using WriteFn = std::function<void(const uint8_t* data, size_t len, bool withoutResponse)>;

  virtual ~TimemoreScale() = default;

  virtual const char* serviceUuid() const = 0;
  virtual const char* notifyCharUuid() const = 0;
  virtual const char* commandCharUuid() const = 0;
  virtual bool writeWithoutResponse() const = 0;

  // Called once, right after the notify subscription succeeds. Override to
  // send a model-specific init sequence (e.g. "set unit to grams"). May use
  // delay() - the manager calls this from the caller's task, not from a
  // NimBLE stack callback, so blocking here is safe.
  virtual void onConnected() {}

  virtual void tare() = 0;
  virtual void setTimer(TimemoreTimerCommand command) = 0;
  virtual bool requestBattery() { return false; }

  // Called by TimemoreScaleManager whenever a notification arrives on
  // notifyCharUuid(); parses it and calls setWeight()/setBattery().
  virtual void handleNotification(const uint8_t* data, size_t len) = 0;

  float getWeight() const { return _weight; }
  float getWeight2() const { return _weight2; }
  uint8_t getBatteryLevel() const { return _battery; }

  void onWeight(WeightCallback cb) { _weightCb = cb; }
  void onBattery(BatteryCallback cb) { _batteryCb = cb; }

  // Internal - called by TimemoreScaleManager, don't call this yourself.
  void _attachWriter(WriteFn fn) { _write = fn; }

 protected:
  void setWeight(float weight, float weight2 = 0.0f) {
    _weight = weight;
    _weight2 = weight2;
    if (_weightCb) _weightCb(weight, weight2);
  }

  void setBattery(uint8_t percent) {
    _battery = percent;
    if (_batteryCb) _batteryCb(percent);
  }

  void writeRaw(const uint8_t* data, size_t len) {
    if (_write) _write(data, len, writeWithoutResponse());
  }

 private:
  float _weight = 0.0f;
  float _weight2 = 0.0f;
  uint8_t _battery = 0;
  WeightCallback _weightCb;
  BatteryCallback _batteryCb;
  WriteFn _write;
};
