#include "ca/protocol.hpp"

namespace ca {
namespace {

const uint8_t kMagic0 = 0xCA;
const uint8_t kMagic1 = 0x53;
const uint16_t kHeaderBytes = 20;
const uint16_t kChecksumBytes = 4;

void put8(uint8_t* out, uint16_t& offset, uint8_t value) {
  out[offset++] = value;
}
void put16(uint8_t* out, uint16_t& offset, uint16_t value) {
  put8(out, offset, static_cast<uint8_t>(value >> 8));
  put8(out, offset, static_cast<uint8_t>(value));
}
void put32(uint8_t* out, uint16_t& offset, uint32_t value) {
  put8(out, offset, static_cast<uint8_t>(value >> 24));
  put8(out, offset, static_cast<uint8_t>(value >> 16));
  put8(out, offset, static_cast<uint8_t>(value >> 8));
  put8(out, offset, static_cast<uint8_t>(value));
}
uint8_t get8(const uint8_t* in, uint16_t& offset) { return in[offset++]; }
uint16_t get16(const uint8_t* in, uint16_t& offset) {
  const uint16_t high = get8(in, offset);
  return static_cast<uint16_t>((high << 8) | get8(in, offset));
}
uint32_t get32(const uint8_t* in, uint16_t& offset) {
  const uint32_t b0 = get8(in, offset);
  const uint32_t b1 = get8(in, offset);
  const uint32_t b2 = get8(in, offset);
  const uint32_t b3 = get8(in, offset);
  return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}
uint32_t crc32(const uint8_t* data, uint16_t length) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint16_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc & 1UL) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
  }
  return crc ^ 0xFFFFFFFFUL;
}
bool validMessageType(MessageType type) {
  return static_cast<uint8_t>(type) <=
         static_cast<uint8_t>(MessageType::ACKNOWLEDGEMENT);
}
} // namespace

EncodeResult BinaryFrameCodec::encode(const ProtocolMessage& message,
                                      MutableFrameView output) const {
  EncodeResult result = {Status(ErrorCode::INVALID_ARGUMENT), 0};
  if (output.data == 0 || message.header.protocolVersion == 0 ||
      message.header.networkId == 0 || message.header.sourceDeviceId == 0 ||
      !validMessageType(message.header.messageType) ||
      message.payloadLength > kMaxProtocolPayloadBytes)
    return result;

  const uint16_t required =
      static_cast<uint16_t>(kHeaderBytes + message.payloadLength + kChecksumBytes);
  if (required > kMaxFrameBytes)
    return result;
  if (output.capacity < required) {
    result.status = Status(ErrorCode::BUFFER_FULL);
    return result;
  }

  uint16_t offset = 0;
  put8(output.data, offset, kMagic0);
  put8(output.data, offset, kMagic1);
  put8(output.data, offset, message.header.protocolVersion);
  put16(output.data, offset, message.header.networkId);
  put8(output.data, offset, static_cast<uint8_t>(message.header.messageType));
  put16(output.data, offset, message.header.sourceDeviceId);
  put16(output.data, offset, message.header.destinationDeviceId);
  put32(output.data, offset, message.header.transactionId);
  put32(output.data, offset, message.header.messageSequence);
  put16(output.data, offset, message.payloadLength);
  for (uint16_t i = 0; i < message.payloadLength; ++i)
    put8(output.data, offset, message.payload[i]);
  put32(output.data, offset, crc32(output.data, offset));

  result.status = Status();
  result.bytesWritten = offset;
  return result;
}

DecodeResult BinaryFrameCodec::decode(FrameView input, ProtocolMessage& message) const {
  DecodeResult result = {Status(ErrorCode::INVALID_ARGUMENT), 0};
  if (input.data == 0 || input.length < kHeaderBytes + kChecksumBytes ||
      input.length > kMaxFrameBytes)
    return result;
  if (input.data[0] != kMagic0 || input.data[1] != kMagic1) {
    result.status = Status(ErrorCode::PROTOCOL_MISMATCH);
    return result;
  }

  const uint16_t payloadLength =
      static_cast<uint16_t>((static_cast<uint16_t>(input.data[18]) << 8) |
                            input.data[19]);
  const uint16_t expectedLength =
      static_cast<uint16_t>(kHeaderBytes + payloadLength + kChecksumBytes);
  if (payloadLength > kMaxProtocolPayloadBytes || input.length != expectedLength) {
    result.status = Status(ErrorCode::PROTOCOL_MISMATCH);
    return result;
  }

  const uint16_t checksumOffset = static_cast<uint16_t>(input.length - kChecksumBytes);
  uint16_t checksumReadOffset = checksumOffset;
  const uint32_t supplied = get32(input.data, checksumReadOffset);
  if (crc32(input.data, checksumOffset) != supplied) {
    result.status = Status(ErrorCode::PROTOCOL_MISMATCH);
    return result;
  }

  ProtocolMessage decoded = {};
  uint16_t offset = 2;
  decoded.header.protocolVersion = get8(input.data, offset);
  decoded.header.networkId = get16(input.data, offset);
  decoded.header.messageType = static_cast<MessageType>(get8(input.data, offset));
  decoded.header.sourceDeviceId = get16(input.data, offset);
  decoded.header.destinationDeviceId = get16(input.data, offset);
  decoded.header.transactionId = get32(input.data, offset);
  decoded.header.messageSequence = get32(input.data, offset);
  decoded.payloadLength = get16(input.data, offset);

  if (decoded.header.protocolVersion == 0 || decoded.header.networkId == 0 ||
      decoded.header.sourceDeviceId == 0 ||
      !validMessageType(decoded.header.messageType)) {
    result.status = Status(ErrorCode::PROTOCOL_MISMATCH);
    return result;
  }
  for (uint16_t i = 0; i < decoded.payloadLength; ++i)
    decoded.payload[i] = get8(input.data, offset);

  message = decoded;
  result.status = Status();
  result.bytesRead = input.length;
  return result;
}

Status DefaultMessageValidator::validate(const ProtocolMessage& message,
                                         const DeviceConfig& localConfig) const {
  if (localConfig.deviceId == 0 || localConfig.networkId == 0 ||
      localConfig.protocolVersion == 0 || message.header.sourceDeviceId == 0 ||
      message.header.networkId != localConfig.networkId ||
      message.header.protocolVersion != localConfig.protocolVersion ||
      !validMessageType(message.header.messageType) ||
      message.payloadLength > kMaxProtocolPayloadBytes)
    return Status(ErrorCode::PROTOCOL_MISMATCH);
  if (message.header.destinationDeviceId != 0 &&
      message.header.destinationDeviceId != localConfig.deviceId)
    return Status(ErrorCode::INVALID_ARGUMENT);
  return Status();
}

Status DefaultMessageValidator::authorizeSender(
    const ProtocolHeader& envelope, const DeviceConfig& localConfig,
    SenderValidation& validation) const {
  validation = SenderValidation::UNVALIDATED;
  if (localConfig.deviceId == 0 || localConfig.networkId == 0 ||
      localConfig.protocolVersion == 0 || envelope.sourceDeviceId == 0 ||
      envelope.sourceDeviceId == localConfig.deviceId ||
      envelope.networkId != localConfig.networkId ||
      envelope.protocolVersion != localConfig.protocolVersion ||
      !validMessageType(envelope.messageType) ||
      (envelope.destinationDeviceId != 0 &&
       envelope.destinationDeviceId != localConfig.deviceId))
    return Status(ErrorCode::PROTOCOL_MISMATCH);

  // Network ID and envelope checks are not cryptographic authentication.
  validation = SenderValidation::NETWORK_MEMBER_VALIDATED;
  return Status();
}

} // namespace ca
