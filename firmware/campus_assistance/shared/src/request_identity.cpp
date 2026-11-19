#include "ca/domain.hpp"

namespace ca {
namespace {

void hashByte(uint32_t& hashValue, uint8_t value) {
  hashValue ^= value;
  hashValue *= 16777619UL;
}

void hashUint16(uint32_t& hashValue, uint16_t value) {
  hashByte(hashValue, static_cast<uint8_t>((value >> 8) & 0xFFU));
  hashByte(hashValue, static_cast<uint8_t>(value & 0xFFU));
}

void hashUint32(uint32_t& hashValue, uint32_t value) {
  hashByte(hashValue, static_cast<uint8_t>((value >> 24) & 0xFFUL));
  hashByte(hashValue, static_cast<uint8_t>((value >> 16) & 0xFFUL));
  hashByte(hashValue, static_cast<uint8_t>((value >> 8) & 0xFFUL));
  hashByte(hashValue, static_cast<uint8_t>(value & 0xFFUL));
}

} // namespace

bool RequestIdentity::isValid(const RequestId& id) {
  // Station IDs are provisioned as nonzero. The LLD does not reserve zero
  // sequence or generation values, so both remain valid identity values.
  return id.originStationId != 0;
}

bool RequestIdentity::equals(const RequestId& lhs, const RequestId& rhs) {
  return lhs.originStationId == rhs.originStationId &&
         lhs.sequence == rhs.sequence && lhs.generation == rhs.generation;
}

uint32_t RequestIdentity::hash(const RequestId& id) {
  // FNV-1a over canonical big-endian fields is independent of host endianness.
  uint32_t value = 2166136261UL;
  hashUint16(value, id.originStationId);
  hashUint32(value, id.sequence);
  hashUint16(value, id.generation);
  return value;
}

} // namespace ca
