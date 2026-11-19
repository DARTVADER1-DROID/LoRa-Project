#pragma once

#include "ca/ports/byte_storage.hpp"
#include "ca/ports/persistence.hpp"

namespace ca {

// Versioned two-slot persistence. A slot becomes visible only after its marker
// is written and synced; the previous valid slot remains untouched until then.
// The byte-store adapter must make sync() durable and report failed writes.
template <uint16_t SlotBytes>
class FixedDualSlotPersistence final : public IPersistence {
  static_assert(SlotBytes > 17, "SlotBytes must hold header, CRC and marker");
  static_assert(SlotBytes <= 32767, "SlotBytes exceeds bounded offset arithmetic");

  enum {
    kHeaderBytes = 12,
    kChecksumBytes = 4,
    kMarkerBytes = 1,
    kMaxPayloadBytes = SlotBytes - kHeaderBytes - kChecksumBytes - kMarkerBytes
  };

  static const uint32_t kMagic = 0x43415350UL; // "CASP"
  static const uint8_t kCommitMarker = 0xA5;

public:
  FixedDualSlotPersistence(IByteStorage& storage, RecordKey key)
      : storage_(storage), key_(key), initialized_(false), scratch_() {}

  Status begin() {
    if (initialized_ || key_ == 0)
      return Status(ErrorCode::INVALID_STATE);
    const Status status = storage_.begin();
    if (!status.ok())
      return status;
    if (static_cast<uint32_t>(storage_.capacity()) <
        static_cast<uint32_t>(SlotBytes) * 2UL)
      return Status(ErrorCode::CONFIGURATION_INVALID);
    initialized_ = true;
    return Status();
  }

  Status load(RecordKey key,
              uint8_t* output,
              uint16_t capacity,
              uint16_t& bytesRead) override {
    bytesRead = 0;
    if (!initialized_)
      return Status(ErrorCode::INVALID_STATE);
    if (key != key_)
      return Status(ErrorCode::NOT_FOUND);

    SlotInfo latest = {};
    const Status latestStatus = findLatest(latest);
    if (!latestStatus.ok())
      return latestStatus;

    SlotInfo verified = {};
    const Status readStatus = readSlot(latest.index, verified);
    if (!readStatus.ok())
      return Status(ErrorCode::STORAGE_FAILURE);
    bytesRead = verified.length;
    if (capacity < verified.length)
      return Status(ErrorCode::BUFFER_FULL);
    if (verified.length > 0 && output == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < verified.length; ++i)
      output[i] = scratch_[kHeaderBytes + i];
    return Status();
  }

  Status storeAtomically(RecordKey key, const uint8_t* data, uint16_t length) override {
    if (!initialized_)
      return Status(ErrorCode::INVALID_STATE);
    if (key != key_ || (length > 0 && data == 0) || length > kMaxPayloadBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);

    SlotInfo latest = {};
    const Status latestStatus = findLatest(latest);
    if (!latestStatus.ok() && latestStatus.code != ErrorCode::NOT_FOUND)
      return latestStatus;

    const uint8_t targetIndex =
        latestStatus.ok() ? static_cast<uint8_t>(latest.index ^ 1U) : 0;
    uint32_t generation = latestStatus.ok() ? latest.generation + 1U : 1U;
    if (generation == 0)
      generation = 1;

    const uint16_t base = static_cast<uint16_t>(targetIndex * SlotBytes);
    for (uint16_t i = 0; i < SlotBytes; ++i)
      scratch_[i] = 0xFF;
    uint16_t offset = 0;
    put32(scratch_, offset, kMagic);
    put16(scratch_, offset, key_);
    put32(scratch_, offset, generation);
    put16(scratch_, offset, length);
    for (uint16_t i = 0; i < length; ++i)
      scratch_[offset++] = data[i];
    const uint32_t checksum = crc32(scratch_, offset);
    put32(scratch_, offset, checksum);
    scratch_[SlotBytes - 1U] = kCommitMarker;

    // The target is always the older/inactive slot. The byte-store contract
    // preserves write order, so the commit marker is the final byte programmed.
    const Status status = storage_.write(base, scratch_, SlotBytes);
    if (!status.ok())
      return status;
    return storage_.sync();
  }

  Status erase(RecordKey key) override {
    if (!initialized_)
      return Status(ErrorCode::INVALID_STATE);
    if (key != key_)
      return Status(ErrorCode::NOT_FOUND);
    for (uint8_t slot = 0; slot < 2; ++slot) {
      const uint16_t base = static_cast<uint16_t>(slot * SlotBytes);
      for (uint16_t i = 0; i < SlotBytes; ++i)
        scratch_[i] = 0xFF;
      const Status status = storage_.write(base, scratch_, SlotBytes);
      if (!status.ok())
        return status;
      const Status synced = storage_.sync();
      if (!synced.ok())
        return synced;
    }
    return Status();
  }

  static uint16_t maxPayloadBytes() { return kMaxPayloadBytes; }

private:
  struct SlotInfo {
    uint8_t index;
    uint32_t generation;
    uint16_t length;
  };

  IByteStorage& storage_;
  RecordKey key_;
  bool initialized_;
  uint8_t scratch_[SlotBytes];

  Status findLatest(SlotInfo& latest) {
    SlotInfo first = {};
    SlotInfo second = {};
    const Status firstStatus = readSlot(0, first);
    const Status secondStatus = readSlot(1, second);

    if (firstStatus.ok() && secondStatus.ok()) {
      const SerialNumberOrder order =
          compareSequence(second.generation, first.generation);
      if (order == SerialNumberOrder::NEWER) {
        latest = second;
        return Status();
      }
      if (order == SerialNumberOrder::OLDER) {
        latest = first;
        return Status();
      }
      return Status(ErrorCode::STORAGE_FAILURE);
    }
    if (firstStatus.ok()) {
      if (secondStatus.code == ErrorCode::STORAGE_FAILURE)
        return Status(ErrorCode::STORAGE_FAILURE);
      latest = first;
      return Status();
    }
    if (secondStatus.ok()) {
      if (firstStatus.code == ErrorCode::STORAGE_FAILURE)
        return Status(ErrorCode::STORAGE_FAILURE);
      latest = second;
      return Status();
    }
    if (firstStatus.code == ErrorCode::STORAGE_FAILURE ||
        secondStatus.code == ErrorCode::STORAGE_FAILURE ||
        firstStatus.code == ErrorCode::CORRUPT_DATA ||
        secondStatus.code == ErrorCode::CORRUPT_DATA)
      return Status(ErrorCode::STORAGE_FAILURE);
    return Status(ErrorCode::NOT_FOUND);
  }

  Status readSlot(uint8_t index, SlotInfo& info) {
    const uint16_t base = static_cast<uint16_t>(index * SlotBytes);
    const Status readStatus = storage_.read(base, scratch_, SlotBytes);
    if (!readStatus.ok())
      return readStatus;
    if (scratch_[SlotBytes - 1U] != kCommitMarker) {
      bool blank = true;
      for (uint16_t i = 0; i < SlotBytes; ++i)
        if (scratch_[i] != 0xFF) {
          blank = false;
          break;
        }
      return blank ? Status(ErrorCode::NOT_FOUND) : Status(ErrorCode::CORRUPT_DATA);
    }

    uint16_t offset = 0;
    const uint32_t magic = get32(scratch_, offset);
    const RecordKey storedKey = get16(scratch_, offset);
    const uint32_t generation = get32(scratch_, offset);
    const uint16_t length = get16(scratch_, offset);
    if (magic != kMagic || storedKey != key_)
      return Status(ErrorCode::CORRUPT_DATA);
    if (generation == 0 || length > kMaxPayloadBytes)
      return Status(ErrorCode::CORRUPT_DATA);

    const uint16_t checksumOffset = static_cast<uint16_t>(kHeaderBytes + length);
    const uint32_t supplied =
        (static_cast<uint32_t>(scratch_[checksumOffset]) << 24) |
        (static_cast<uint32_t>(scratch_[checksumOffset + 1U]) << 16) |
        (static_cast<uint32_t>(scratch_[checksumOffset + 2U]) << 8) |
        static_cast<uint32_t>(scratch_[checksumOffset + 3U]);
    if (crc32(scratch_, checksumOffset) != supplied)
      return Status(ErrorCode::CORRUPT_DATA);

    info.index = index;
    info.generation = generation;
    info.length = length;
    return Status();
  }

  static void put16(uint8_t* output, uint16_t& offset, uint16_t value) {
    output[offset++] = static_cast<uint8_t>(value >> 8);
    output[offset++] = static_cast<uint8_t>(value);
  }

  static void put32(uint8_t* output, uint16_t& offset, uint32_t value) {
    output[offset++] = static_cast<uint8_t>(value >> 24);
    output[offset++] = static_cast<uint8_t>(value >> 16);
    output[offset++] = static_cast<uint8_t>(value >> 8);
    output[offset++] = static_cast<uint8_t>(value);
  }

  static uint16_t get16(const uint8_t* input, uint16_t& offset) {
    const uint16_t high = input[offset++];
    return static_cast<uint16_t>((high << 8) | input[offset++]);
  }

  static uint32_t get32(const uint8_t* input, uint16_t& offset) {
    const uint32_t b0 = input[offset++];
    const uint32_t b1 = input[offset++];
    const uint32_t b2 = input[offset++];
    const uint32_t b3 = input[offset++];
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  }

  static uint32_t crc32(const uint8_t* data, uint16_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint16_t i = 0; i < length; ++i) {
      crc ^= data[i];
      for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 1UL) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
  }
};

} // namespace ca
