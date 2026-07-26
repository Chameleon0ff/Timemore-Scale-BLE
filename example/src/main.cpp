#include <Arduino.h>
#include <Preferences.h>
#include <TimemoreBLE.h>

TimemoreScaleManager scaleManager;
Preferences prefs;

float currentWeight = 0.0f;
uint8_t currentBattery = 0;
bool timerRunning = false;
uint32_t timerStartMs = 0;
uint32_t timerAccumulatedMs = 0;
uint32_t lastTimerPrintMs = 0;
uint32_t lastBatteryRequestMs = 0;

String commandBuffer;

void attachScaleCallbacks(TimemoreScale* scale) {
  if (!scale) {
    return;
  }

  scale->onWeight([](float w, float /*w2*/) {
    currentWeight = w;
  });

  scale->onBattery([](uint8_t p) {
    currentBattery = p;
    Serial.printf("Battery: %u%%\n", currentBattery);
  });
}

void printHelp() {
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("  TARE  - tare scale");
  Serial.println("  START - start timer");
  Serial.println("  STOP  - stop timer");
  Serial.println("  STATUS- print current state");
  Serial.println();
}

uint32_t timerElapsedMs() {
  if (timerRunning) {
    return timerAccumulatedMs + (millis() - timerStartMs);
  }
  return timerAccumulatedMs;
}

void printStatus() {
  Serial.printf("Connected: %s\n", scaleManager.isConnected() ? "YES" : "NO");
  Serial.printf("Weight: %.1f g\n", currentWeight);
  Serial.printf("Battery: %u%%\n", currentBattery);

  const uint32_t elapsed = timerElapsedMs();
  const uint32_t sec = elapsed / 1000;
  const uint32_t mm = sec / 60;
  const uint32_t ss = sec % 60;
  Serial.printf("Timer: %s %02lu:%02lu\n", timerRunning ? "RUN" : "STOP", mm, ss);
}

void handleCommand(const String& input) {
  String cmd = input;
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.length() == 0) {
    return;
  }

  if (cmd == "TARE") {
    if (!scaleManager.isConnected() || !scaleManager.scale()) {
      Serial.println("Scale is not connected");
      return;
    }

    scaleManager.scale()->tare();
    Serial.println("TARE command sent");
    return;
  }

  if (cmd == "START") {
    if (!timerRunning) {
      timerStartMs = millis();
      timerRunning = true;
      Serial.println("Local timer started");
    } else {
      Serial.println("Local timer already running");
    }

    if (scaleManager.isConnected() && scaleManager.scale()) {
      scaleManager.scale()->setTimer(TimemoreTimerCommand::START);
      Serial.println("Scale timer START command sent");
    }
    return;
  }

  if (cmd == "STOP") {
    if (timerRunning) {
      timerAccumulatedMs += (millis() - timerStartMs);
      timerRunning = false;
      Serial.println("Local timer stopped");
    } else {
      Serial.println("Local timer already stopped");
    }

    timerAccumulatedMs = 0;
    timerStartMs = 0;
    Serial.println("Local timer reset to 00:00");

    if (scaleManager.isConnected() && scaleManager.scale()) {
      scaleManager.scale()->setTimer(TimemoreTimerCommand::STOP);
      Serial.println("Scale timer STOP command sent");
      scaleManager.scale()->setTimer(TimemoreTimerCommand::RESET);
      Serial.println("Scale timer RESET command sent");
    }
    return;
  }

  if (cmd == "STATUS") {
    printStatus();
    return;
  }

  if (cmd == "HELP") {
    printHelp();
    return;
  }

  Serial.printf("Unknown command: %s\n", cmd.c_str());
  printHelp();
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      handleCommand(commandBuffer);
      commandBuffer = "";
      continue;
    }

    commandBuffer += ch;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  prefs.begin("timemore", false);

  const String savedAddr = prefs.getString("addr", "");
  const uint8_t savedType = prefs.getUChar("atype", 0);
  const uint8_t savedModelRaw = prefs.getUChar("model", 0);
  if (savedAddr.length() > 0 && savedModelRaw <= static_cast<uint8_t>(TimemoreModel::CLASSIC)) {
    scaleManager.setPreferredPeer(std::string(savedAddr.c_str()),
                                  savedType,
                                  static_cast<TimemoreModel>(savedModelRaw));
    Serial.printf("Loaded saved peer: %s (type=%u, model=%u)\n",
                  savedAddr.c_str(),
                  savedType,
                  savedModelRaw);
  }

  scaleManager.onConnectionChange([](bool connected) {
    Serial.printf("Scale %s\n", connected ? "connected" : "disconnected");

    if (connected && scaleManager.scale()) {
      attachScaleCallbacks(scaleManager.scale());
      currentBattery = scaleManager.scale()->getBatteryLevel();

      const std::string& addr = scaleManager.connectedAddress();
      if (!addr.empty()) {
        prefs.putString("addr", addr.c_str());
        prefs.putUChar("atype", scaleManager.connectedAddressType());
        prefs.putUChar("model", static_cast<uint8_t>(scaleManager.model()));
      }
    }
  });

  Serial.println();
  Serial.println("Timemore APP boot");
  Serial.println("Scanning and connecting to Timemore scale...");

  if (scaleManager.connect(15000)) {
    Serial.println("Scale connected");

    const std::string& addr = scaleManager.connectedAddress();
    if (!addr.empty()) {
      prefs.putString("addr", addr.c_str());
      prefs.putUChar("atype", scaleManager.connectedAddressType());
      prefs.putUChar("model", static_cast<uint8_t>(scaleManager.model()));
    }
  } else {
    Serial.println("Scale not found within timeout, active reconnect will continue in loop");
  }

  if (scaleManager.scale()) {
    attachScaleCallbacks(scaleManager.scale());
    currentBattery = scaleManager.scale()->getBatteryLevel();
  }

  printHelp();
  printStatus();
}

void loop() {
  scaleManager.update();
  readSerialCommands();

  if (scaleManager.isConnected() && scaleManager.scale()) {
    // Keep a last known battery snapshot even if battery notifications are sparse.
    currentBattery = scaleManager.scale()->getBatteryLevel();

    const uint32_t now = millis();
    if (now - lastBatteryRequestMs >= 10000) {
      lastBatteryRequestMs = now;
      scaleManager.scale()->requestBattery();
    }
  }

  if (millis() - lastTimerPrintMs >= 1000) {
    lastTimerPrintMs = millis();

    const uint32_t elapsed = timerElapsedMs();
    const uint32_t sec = elapsed / 1000;
    const uint32_t mm = sec / 60;
    const uint32_t ss = sec % 60;

    Serial.printf("W: %.1f g | B: %u%% | T: %02lu:%02lu%s\n",
                  currentWeight,
                  currentBattery,
                  mm,
                  ss,
                  timerRunning ? " (RUN)" : "");
  }

  delay(20);
}
