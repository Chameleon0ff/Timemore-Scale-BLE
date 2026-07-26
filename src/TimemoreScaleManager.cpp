#include "TimemoreScaleManager.h"
#include "TimemoreBasicScale.h"
#include "TimemoreClassicScale.h"
#include "TimemoreDotScale.h"
#include "TimemoreUtil.h"

namespace {
// NimBLE's C-style notify callback has no user-data pointer, so we route
// through a single global. Fine in practice: there's only ever one BLE
// central role active on an ESP32 at a time in this library's use case.
TimemoreScaleManager* g_notifyOwner = nullptr;

TimemoreModel detectModelFromAdvertisedServices(const NimBLEAdvertisedDevice* device) {
  if (device == nullptr) return TimemoreModel::NONE;

  const NimBLEUUID classicSvc("0000181d-0000-1000-8000-00805f9b34fb");
  if (device->isAdvertisingService(classicSvc)) {
    return TimemoreModel::CLASSIC;
  }

  const NimBLEUUID framedSvc("0000fff0-0000-1000-8000-00805f9b34fb");
  if (device->isAdvertisingService(framedSvc)) {
    // FFF0 family (DOT/Basic variants) shares the same command channel.
    return TimemoreModel::DOT;
  }

  return TimemoreModel::NONE;
}

void onNotify(NimBLERemoteCharacteristic* /*chr*/, uint8_t* data, size_t length, bool /*isNotify*/) {
  if (g_notifyOwner != nullptr && g_notifyOwner->scale() != nullptr) {
    g_notifyOwner->scale()->handleNotification(data, length);
  }
}
}  // namespace

TimemoreScaleManager::TimemoreScaleManager() : _scanCallbacks(this), _clientCallbacks(this) {}

TimemoreScaleManager::~TimemoreScaleManager() {
  disconnect();
}

TimemoreModel TimemoreScaleManager::detectModel(const std::string& name) {
  if (TimemoreDotScale::matches(name)) return TimemoreModel::DOT;
  if (TimemoreBasicScale::matches(name)) return TimemoreModel::BASIC;
  if (TimemoreClassicScale::matches(name)) return TimemoreModel::CLASSIC;
  return TimemoreModel::NONE;
}

std::unique_ptr<TimemoreScale> TimemoreScaleManager::createScale(TimemoreModel model) {
  switch (model) {
    case TimemoreModel::DOT:
      return std::unique_ptr<TimemoreScale>(new TimemoreDotScale());
    case TimemoreModel::BASIC:
      return std::unique_ptr<TimemoreScale>(new TimemoreBasicScale());
    case TimemoreModel::CLASSIC:
      return std::unique_ptr<TimemoreScale>(new TimemoreClassicScale());
    default:
      return nullptr;
  }
}

void TimemoreScaleManager::ScanCallbacks::onResult(const NimBLEAdvertisedDevice* device) {
  if (_owner->_foundMatch) return;  // already matched, ignore the rest

  TimemoreModel candidate = TimemoreModel::NONE;
  const std::string addr = device->getAddress().toString();
  bool preferredMatch = false;

  if (!_owner->_preferredAddress.empty() &&
      timemoreToLower(addr) == timemoreToLower(_owner->_preferredAddress)) {
    // Known peer may advertise with minimal payload outside pairing mode.
    preferredMatch = true;
    candidate = _owner->_preferredModel;
  }

  if (device->haveName()) {
    if (candidate == TimemoreModel::NONE) {
      candidate = TimemoreScaleManager::detectModel(device->getName());
    }
  }
  if (candidate == TimemoreModel::NONE) {
    candidate = detectModelFromAdvertisedServices(device);
  }

  if (candidate == TimemoreModel::NONE && !preferredMatch) return;

  NimBLEDevice::getScan()->stop();
  _owner->_foundMatch = true;
  _owner->_foundAddress = addr;
  _owner->_foundAddressType = device->getAddress().getType();
  _owner->_foundModel = candidate;
}

void TimemoreScaleManager::ClientCallbacks::onDisconnect(NimBLEClient* /*pClient*/, int /*reason*/) {
  if (g_notifyOwner == _owner) {
    g_notifyOwner = nullptr;
  }
  _owner->_connected = false;
  if (_owner->_connectionCb) _owner->_connectionCb(false);
}

void TimemoreScaleManager::setPreferredPeer(const std::string& address,
                                            uint8_t addressType,
                                            TimemoreModel model) {
  _preferredAddress = address;
  _preferredAddressType = addressType;
  _preferredModel = model;
}

void TimemoreScaleManager::clearPreferredPeer() {
  _preferredAddress.clear();
  _preferredAddressType = 0;
  _preferredModel = TimemoreModel::NONE;
}

bool TimemoreScaleManager::connect(uint32_t timeoutMs) {
  if (_connected) {
    return true;
  }

  static bool nimbleInitDone = false;
  if (!nimbleInitDone) {
    NimBLEDevice::init("");

    // Match native app behavior better: bond with the scale so it can
    // keep reconnecting outside explicit pairing mode.
    NimBLEDevice::setSecurityAuth(true, false, false);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    nimbleInitDone = true;
  }

  if (_client != nullptr) {
    NimBLEClient* staleClient = _client;
    _client = nullptr;
    staleClient->setClientCallbacks(nullptr, false);
    if (staleClient->isConnected()) {
      staleClient->disconnect();
    }
    NimBLEDevice::deleteClient(staleClient);
  }

  auto cleanupFailedClient = [&]() {
    if (_client == nullptr) return;
    NimBLEClient* failedClient = _client;
    _client = nullptr;
    failedClient->setClientCallbacks(nullptr, false);
    if (failedClient->isConnected()) {
      failedClient->disconnect();
    }
    NimBLEDevice::deleteClient(failedClient);
    _scale.reset();
    _model = TimemoreModel::NONE;
  };

  auto tryConnectTarget = [&](const std::string& targetAddress,
                              uint8_t targetAddressType,
                              TimemoreModel hintedModel) -> bool {
    _scale.reset();
    _model = TimemoreModel::NONE;

    _client = NimBLEDevice::createClient();
    _client->setClientCallbacks(&_clientCallbacks, false);

    const uint8_t typeCandidates[5] = {targetAddressType, 0, 1, 2, 3};
    bool connected = false;
    for (uint8_t i = 0; i < 5; i++) {
      const uint8_t t = typeCandidates[i];
      bool duplicate = false;
      for (uint8_t j = 0; j < i; j++) {
        if (typeCandidates[j] == t) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      if (_client->connect(NimBLEAddress(targetAddress, t))) {
        connected = true;
        break;
      }
    }

    if (!connected) {
      cleanupFailedClient();
      return false;
    }

    // Attempt to secure/pair so next reconnect works when scale advertises
    // only for known/bonded peers.
    _client->secureConnection();

    NimBLERemoteService* classicSvc =
        _client->getService("0000181d-0000-1000-8000-00805f9b34fb");
    NimBLERemoteService* framedSvc =
        _client->getService("0000fff0-0000-1000-8000-00805f9b34fb");

    TimemoreModel resolvedModel = TimemoreModel::NONE;
    NimBLERemoteService* service = nullptr;
    const char* notifyUuid = nullptr;
    const char* cmdUuid = nullptr;

    if (hintedModel == TimemoreModel::CLASSIC && classicSvc) {
      resolvedModel = TimemoreModel::CLASSIC;
      service = classicSvc;
      notifyUuid = "00002a9d-0000-1000-8000-00805f9b34fb";
      cmdUuid = "553f4e49-bf21-4468-9c6c-0e4fb5b17697";
    } else if ((hintedModel == TimemoreModel::DOT || hintedModel == TimemoreModel::BASIC) &&
               framedSvc) {
      resolvedModel = hintedModel;
      service = framedSvc;
      notifyUuid = "0000fff1-0000-1000-8000-00805f9b34fb";
      cmdUuid = "0000fff2-0000-1000-8000-00805f9b34fb";
    } else if (classicSvc) {
      resolvedModel = TimemoreModel::CLASSIC;
      service = classicSvc;
      notifyUuid = "00002a9d-0000-1000-8000-00805f9b34fb";
      cmdUuid = "553f4e49-bf21-4468-9c6c-0e4fb5b17697";
    } else if (framedSvc) {
      resolvedModel = (hintedModel == TimemoreModel::BASIC) ? TimemoreModel::BASIC : TimemoreModel::DOT;
      service = framedSvc;
      notifyUuid = "0000fff1-0000-1000-8000-00805f9b34fb";
      cmdUuid = "0000fff2-0000-1000-8000-00805f9b34fb";
    }

    if (resolvedModel == TimemoreModel::NONE || service == nullptr) {
      cleanupFailedClient();
      return false;
    }

    _scale = createScale(resolvedModel);
    if (!_scale) {
      cleanupFailedClient();
      return false;
    }
    _model = resolvedModel;

    NimBLERemoteCharacteristic* notifyChar = service->getCharacteristic(notifyUuid);
    NimBLERemoteCharacteristic* cmdChar = service->getCharacteristic(cmdUuid);
    if (!notifyChar || !cmdChar) {
      cleanupFailedClient();
      return false;
    }

    g_notifyOwner = this;
    bool preferNotify = true;
    if (!notifyChar->canNotify() && notifyChar->canIndicate()) {
      preferNotify = false;
    }

    if (!notifyChar->subscribe(preferNotify, onNotify)) {
      if (g_notifyOwner == this) {
        g_notifyOwner = nullptr;
      }
      cleanupFailedClient();
      return false;
    }

    _scale->_attachWriter([cmdChar](const uint8_t* data, size_t len, bool withoutResponse) {
      cmdChar->writeValue(data, len, /*response=*/!withoutResponse);
    });

    _scale->onConnected();

    _connected = true;
    _reconnectArmed = true;
    _lastReconnectAttemptMs = millis();
    const NimBLEAddress connectedPeer = _client->getPeerAddress();
    _connectedAddress = connectedPeer.toString();
    _connectedAddressType = connectedPeer.getType();

    _preferredAddress = _connectedAddress;
    _preferredAddressType = _connectedAddressType;
    _preferredModel = resolvedModel;

    if (_connectionCb) _connectionCb(true);
    return true;
  };

  // 1) Try known peer first (works even if the scale hides name outside pairing mode).
  if (!_preferredAddress.empty()) {
    if (tryConnectTarget(_preferredAddress, _preferredAddressType, _preferredModel)) {
      return true;
    }
  }

  // 2) Fallback: scan and discover target.
  _foundMatch = false;
  _foundAddress.clear();
  _foundAddressType = 0;
  _foundModel = TimemoreModel::NONE;

  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setScanCallbacks(&_scanCallbacks);
  scanner->setActiveScan(true);
  scanner->setInterval(45);
  scanner->setWindow(45);
  scanner->start(timeoutMs, false);

  const uint32_t waitMs = timeoutMs + 500;
  const uint32_t startMs = millis();
  while (!_foundMatch && static_cast<uint32_t>(millis() - startMs) < waitMs) {
    delay(20);
  }

  if (!_foundMatch || _foundAddress.empty()) {
    scanner->stop();
    return false;
  }

  return tryConnectTarget(_foundAddress, _foundAddressType, _foundModel);
}

void TimemoreScaleManager::disconnect() {
  if (g_notifyOwner == this) {
    g_notifyOwner = nullptr;
  }

  const bool wasConnected = _connected;

  if (_client != nullptr) {
    NimBLEClient* client = _client;
    _client = nullptr;
    client->setClientCallbacks(nullptr, false);
    if (client->isConnected()) {
      client->disconnect();
    }
    NimBLEDevice::deleteClient(client);
  }

  _scale.reset();
  _model = TimemoreModel::NONE;
  _connected = false;
  _reconnectArmed = false;

  if (wasConnected && _connectionCb) {
    _connectionCb(false);
  }
}

void TimemoreScaleManager::update() {
  if (_connected && (_client == nullptr || !_client->isConnected())) {
    _connected = false;
    if (_connectionCb) {
      _connectionCb(false);
    }
  }

  if (_connected || !_autoReconnect) {
    return;
  }

  const uint32_t elapsed = static_cast<uint32_t>(millis() - _lastReconnectAttemptMs);
  if (elapsed < _reconnectIntervalMs) {
    return;
  }

  _lastReconnectAttemptMs = millis();
  connect(_reconnectTimeoutMs);
}
