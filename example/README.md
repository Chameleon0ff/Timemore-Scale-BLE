This folder contains a standalone PlatformIO project for ESP32-WROOM-32 (`esp32dev`) using Arduino framework.

## Features
- Connects to Timemore scale over BLE.
- Reads and prints weight and battery level.
- Serial command `TARE` sends tare command to scale.
- Serial commands `START` and `STOP` control timer:
  - local timer on ESP32,
  - timer command on scale (if connected).

## Serial commands
- `TARE`
- `START`
- `STOP`
- `STATUS`
- `HELP`

