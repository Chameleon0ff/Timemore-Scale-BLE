#include "TimemoreProtocol.h"
#include <string.h>

namespace TimemoreProtocol {

uint16_t crc16Ibm(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

size_t buildFrame(uint8_t opcode, uint8_t cmdId, const uint8_t* data, size_t dataLen, uint8_t* out,
                   size_t outCapacity) {
  const size_t frameLen = 8 + dataLen;
  if (outCapacity < frameLen) return 0;

  out[0] = 0xA5;
  out[1] = 0x5A;
  out[2] = opcode;
  out[3] = cmdId;
  out[4] = static_cast<uint8_t>((dataLen >> 8) & 0xFF);
  out[5] = static_cast<uint8_t>(dataLen & 0xFF);
  if (dataLen > 0 && data != nullptr) {
    memcpy(out + 6, data, dataLen);
  }

  const uint16_t crc = crc16Ibm(out, 6 + dataLen);
  out[6 + dataLen] = static_cast<uint8_t>((crc >> 8) & 0xFF);
  out[7 + dataLen] = static_cast<uint8_t>(crc & 0xFF);

  return frameLen;
}

ParsedFrame tryParseFrame(const uint8_t* raw, size_t rawLen, bool validateCrc) {
  ParsedFrame result;

  if (rawLen < 8) return result;
  if (raw[0] != 0xA5 || raw[1] != 0x5A) return result;

  const uint8_t opcode = raw[2];
  const uint8_t cmdId = raw[3];
  const size_t dataLen = (static_cast<size_t>(raw[4]) << 8) | raw[5];

  if (rawLen < 8 + dataLen) return result;

  if (validateCrc) {
    const uint16_t received = (static_cast<uint16_t>(raw[6 + dataLen]) << 8) | raw[7 + dataLen];
    const uint16_t calculated = crc16Ibm(raw, 6 + dataLen);
    if (received != calculated) return result;
  }

  result.valid = true;
  result.opcode = opcode;
  result.cmdId = cmdId;
  result.data = raw + 6;
  result.dataLen = dataLen;
  return result;
}

}  // namespace TimemoreProtocol
