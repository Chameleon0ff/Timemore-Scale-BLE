#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <memory>
#include "TimemoreScale.h"

enum class TimemoreModel { NONE, DOT, BASIC, CLASSIC };

// Owns the NimBLE scan/connect/subscribe flow and routes notifications to
// the right TimemoreScale subclass. This is the only class in the library
// that includes NimBLEDevice.h - scale classes stay BLE-agnostic.
//
// IMPORTANT: connect() is blocking (it scans, then delay()s during the
// model's init sequence). Call it from setup() or a dedicated FreeRTOS
// task - never from inside a NimBLE stack callback (onConnect/onResult/
// notify callback etc.), or you risk stalling the BLE host task.
class TimemoreScaleManager {
 public:
  using ConnectionCallback = std::function<void(bool connected)>;

  TimemoreScaleManager();
  ~TimemoreScaleManager();

  // Scans for up to `timeoutMs`, connects to the first device whose name
  // matches a known Timemore model, subscribes to notifications and runs
  // the model's init sequence. Returns false on timeout/failure.
  bool connect(uint32_t timeoutMs = 15000);

  void disconnect();
  bool isConnected() const { return _connected; }

  TimemoreScale* scale() { return _scale.get(); }
  TimemoreModel model() const { return _model; }

  // Optional hint for faster/stabler reconnects without relying on scan name visibility.
  void setPreferredPeer(const std::string& address, uint8_t addressType, TimemoreModel model);
  void clearPreferredPeer();

  const std::string& connectedAddress() const { return _connectedAddress; }
  uint8_t connectedAddressType() const { return _connectedAddressType; }

  void onConnectionChange(ConnectionCallback cb) { _connectionCb = cb; }

  // Call periodically (e.g. every loop() iteration / task tick). If the
  // scale disconnects unexpectedly after a successful connection, this method
  // attempts periodic blocking reconnects.
  void update();

 private:
  static TimemoreModel detectModel(const std::string& name);
  std::unique_ptr<TimemoreScale> createScale(TimemoreModel model);

  class ScanCallbacks : public NimBLEScanCallbacks {
   public:
    explicit ScanCallbacks(TimemoreScaleManager* owner) : _owner(owner) {}
    void onResult(const NimBLEAdvertisedDevice* device) override;

   private:
    TimemoreScaleManager* _owner;
  };

  class ClientCallbacks : public NimBLEClientCallbacks {
   public:
    explicit ClientCallbacks(TimemoreScaleManager* owner) : _owner(owner) {}
    void onDisconnect(NimBLEClient* pClient, int reason) override;

   private:
    TimemoreScaleManager* _owner;
  };

  NimBLEClient* _client = nullptr;
  std::unique_ptr<TimemoreScale> _scale;
  TimemoreModel _model = TimemoreModel::NONE;
  bool _connected = false;
  ConnectionCallback _connectionCb;

  ScanCallbacks _scanCallbacks;
  ClientCallbacks _clientCallbacks;

  // Set by ScanCallbacks::onResult() while a scan is in flight.
  bool _foundMatch = false;
  std::string _foundAddress;
  uint8_t _foundAddressType = 0;
  TimemoreModel _foundModel = TimemoreModel::NONE;

  bool _autoReconnect = true;
  bool _reconnectArmed = false;
  uint32_t _reconnectIntervalMs = 3000;
  uint32_t _reconnectTimeoutMs = 8000;
  uint32_t _lastReconnectAttemptMs = 0;

  std::string _preferredAddress;
  uint8_t _preferredAddressType = 0;
  TimemoreModel _preferredModel = TimemoreModel::NONE;

  std::string _connectedAddress;
  uint8_t _connectedAddressType = 0;
};
