#include "CampusAssistance.h"

#include <assert.h>
#include <string.h>

using namespace ca;

int main() {
  BinaryFrameCodec codec;
  ProtocolMessage message = {};
  message.header.protocolVersion = 1;
  message.header.networkId = 7;
  message.header.messageType = MessageType::REQUEST;
  message.header.sourceDeviceId = 2;
  message.header.destinationDeviceId = 0;
  message.header.transactionId = 0x10203040UL;
  message.header.messageSequence = 9;
  message.payloadLength = 4;
  message.payload[0] = 0x00;
  message.payload[1] = 0x7F;
  message.payload[2] = 0x80;
  message.payload[3] = 0xFF;

  uint8_t bytes[kMaxFrameBytes] = {};
  EncodeResult encoded = codec.encode(message, {bytes, sizeof(bytes)});
  assert(encoded.status.ok());
  assert(encoded.bytesWritten == 28);

  ProtocolMessage decoded = {};
  DecodeResult result = codec.decode({bytes, encoded.bytesWritten}, decoded);
  assert(result.status.ok());
  assert(result.bytesRead == encoded.bytesWritten);
  assert(decoded.header.networkId == 7);
  assert(decoded.header.transactionId == 0x10203040UL);
  assert(decoded.header.messageSequence == 9);
  assert(decoded.payloadLength == 4);
  assert(memcmp(decoded.payload, message.payload, 4) == 0);

  MutableFrameView tooSmall = {bytes, 10};
  assert(codec.encode(message, tooSmall).status.code == ErrorCode::BUFFER_FULL);

  bytes[5] ^= 0x01;
  assert(codec.decode({bytes, encoded.bytesWritten}, decoded).status.code ==
         ErrorCode::PROTOCOL_MISMATCH);
  bytes[5] ^= 0x01;

  uint8_t trailing[kMaxFrameBytes] = {};
  memcpy(trailing, bytes, encoded.bytesWritten);
  trailing[encoded.bytesWritten] = 0;
  assert(
      codec.decode({trailing, static_cast<uint16_t>(encoded.bytesWritten + 1)}, decoded)
          .status.code == ErrorCode::PROTOCOL_MISMATCH);

  DefaultMessageValidator validator;
  DeviceConfig local = {};
  local.deviceId = 1;
  local.networkId = 7;
  local.protocolVersion = 1;
  assert(validator.validate(message, local).ok());
  SenderValidation sender = SenderValidation::UNVALIDATED;
  assert(validator.authorizeSender(message.header, local, sender).ok());
  assert(sender == SenderValidation::NETWORK_MEMBER_VALIDATED);

  local.networkId = 8;
  assert(validator.validate(message, local).code == ErrorCode::PROTOCOL_MISMATCH);
  return 0;
}
