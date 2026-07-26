#pragma once
#include <Arduino.h>

// Binary framing used by the Timemore DOT and Basic 3 scales:
//
//   [0xA5 0x5A][opcode:1][cmdId:1][len:2 BE][data:len][crc16:2 BE]
//
// The CRC is the classic CRC16/IBM (aka CRC16/ARC, poly 0xA001, reflected)
// computed over everything before it. This file is a 1:1 port of the
// protocol logic from the `timemore-ble` TypeScript library / the original
// Beanconqueror source - no ESP32/BLE specifics here on purpose, so it can
// be unit-tested off-target if needed.
namespace TimemoreProtocol {

struct ParsedFrame {
  bool valid = false;
  uint8_t opcode = 0;
  uint8_t cmdId = 0;
  // Points into the buffer passed to tryParseFrame() - only valid as long
  // as that buffer is.
  const uint8_t* data = nullptr;
  size_t dataLen = 0;
};

uint16_t crc16Ibm(const uint8_t* data, size_t len);

// Writes the framed command into `out` (caller-provided buffer, must be at
// least 8 + dataLen bytes). Returns the total frame length, or 0 if
// `outCapacity` is too small.
size_t buildFrame(uint8_t opcode, uint8_t cmdId, const uint8_t* data, size_t dataLen, uint8_t* out,
                   size_t outCapacity);

// Parses a raw notification payload without allocating.
// @param validateCrc Basic 3 validates the CRC of incoming frames; DOT does
//                     not send a reliable one, so pass false for that model.
ParsedFrame tryParseFrame(const uint8_t* raw, size_t rawLen, bool validateCrc);

}  // namespace TimemoreProtocol
