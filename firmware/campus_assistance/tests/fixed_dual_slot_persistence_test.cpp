#include "ca/storage/fixed_dual_slot_persistence.hpp"

#include <assert.h>
#include <stdint.h>

using namespace ca;

template <uint16_t Capacity> class FakeByteStorage final : public IByteStorage {
public:
  FakeByteStorage() : writeBudget_(-1), initialized_(false) {
    for (uint16_t i = 0; i < Capacity; ++i)
      bytes_[i] = 0xFF;
  }

  Status begin() override {
    initialized_ = true;
    return Status();
  }

  uint16_t capacity() const override { return Capacity; }

  Status read(uint16_t offset, uint8_t* output, uint16_t length) const override {
    if (!initialized_ || (length > 0 && output == 0) ||
        static_cast<uint32_t>(offset) + length > Capacity)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i)
      output[i] = bytes_[offset + i];
    return Status();
  }

  Status write(uint16_t offset, const uint8_t* data, uint16_t length) override {
    if (!initialized_ || (length > 0 && data == 0) ||
        static_cast<uint32_t>(offset) + length > Capacity)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i) {
      if (writeBudget_ == 0)
        return Status(ErrorCode::STORAGE_FAILURE);
      bytes_[offset + i] = data[i];
      if (writeBudget_ > 0)
        --writeBudget_;
    }
    return Status();
  }

  Status sync() override { return Status(); }
  void failAfterWrites(int32_t count) { writeBudget_ = count; }
  uint8_t* bytes() { return bytes_; }

private:
  uint8_t bytes_[Capacity];
  int32_t writeBudget_;
  bool initialized_;
};

int main() {
  const RecordKey key = 9;
  FakeByteStorage<256> interruptedFirstWriteStorage;
  FixedDualSlotPersistence<128> interruptedFirstWrite(
      interruptedFirstWriteStorage, key);
  assert(interruptedFirstWrite.begin().ok());
  const uint8_t initialValue[] = {4, 5, 6};
  interruptedFirstWriteStorage.failAfterWrites(20);
  assert(interruptedFirstWrite
             .storeAtomically(key, initialValue, sizeof(initialValue))
             .code == ErrorCode::STORAGE_FAILURE);
  interruptedFirstWriteStorage.failAfterWrites(-1);
  uint8_t initialOutput[8] = {};
  uint16_t initialBytesRead = 0;
  assert(interruptedFirstWrite
             .load(key, initialOutput, sizeof(initialOutput), initialBytesRead)
             .code == ErrorCode::STORAGE_FAILURE);

  FakeByteStorage<256> storage;
  FixedDualSlotPersistence<128> persistence(storage, key);
  assert(persistence.begin().ok());

  uint8_t output[32] = {};
  uint16_t bytesRead = 0;
  assert(persistence.load(key, output, sizeof(output), bytesRead).code ==
         ErrorCode::NOT_FOUND);

  const uint8_t oldValue[] = {1, 2, 3, 4, 5};
  assert(persistence.storeAtomically(key, oldValue, sizeof(oldValue)).ok());
  assert(persistence.load(key, output, sizeof(output), bytesRead).ok());
  assert(bytesRead == sizeof(oldValue));
  for (uint8_t i = 0; i < sizeof(oldValue); ++i)
    assert(output[i] == oldValue[i]);

  const uint8_t newValue[] = {9, 8, 7, 6, 5, 4};
  storage.failAfterWrites(20); // Invalidate the other slot, then tear its body.
  assert(persistence.storeAtomically(key, newValue, sizeof(newValue)).code ==
         ErrorCode::STORAGE_FAILURE);
  storage.failAfterWrites(-1);
  assert(persistence.load(key, output, sizeof(output), bytesRead).ok());
  assert(bytesRead == sizeof(oldValue));
  for (uint8_t i = 0; i < sizeof(oldValue); ++i)
    assert(output[i] == oldValue[i]);

  assert(persistence.storeAtomically(key, newValue, sizeof(newValue)).ok());
  assert(persistence.load(key, output, 2, bytesRead).code == ErrorCode::BUFFER_FULL);
  assert(bytesRead == sizeof(newValue));
  assert(persistence.load(key, output, sizeof(output), bytesRead).ok());
  assert(bytesRead == sizeof(newValue));
  for (uint8_t i = 0; i < sizeof(newValue); ++i)
    assert(output[i] == newValue[i]);

  const uint8_t thirdValue[] = {3, 1, 4, 1, 5, 9, 2};
  storage.failAfterWrites(20); // Tear the older slot while the latest stays intact.
  assert(persistence.storeAtomically(key, thirdValue, sizeof(thirdValue)).code ==
         ErrorCode::STORAGE_FAILURE);
  storage.failAfterWrites(-1);
  assert(persistence.load(key, output, sizeof(output), bytesRead).ok());
  assert(bytesRead == sizeof(newValue));
  for (uint8_t i = 0; i < sizeof(newValue); ++i)
    assert(output[i] == newValue[i]);

  assert(persistence.storeAtomically(key, thirdValue, sizeof(thirdValue)).ok());
  assert(persistence.load(key, output, sizeof(output), bytesRead).ok());
  assert(bytesRead == sizeof(thirdValue));
  for (uint8_t i = 0; i < sizeof(thirdValue); ++i)
    assert(output[i] == thirdValue[i]);

  // A committed slot with a bad CRC is corruption, not an interrupted write.
  // Fail closed rather than roll back the request-ID allocator.
  storage.bytes()[12] ^= 0x01;
  assert(persistence.load(key, output, sizeof(output), bytesRead).code ==
         ErrorCode::STORAGE_FAILURE);

  // Damaging the other slot marker must never reinterpret the store as empty.
  storage.bytes()[255] ^= 0x01;
  assert(persistence.load(key, output, sizeof(output), bytesRead).code ==
         ErrorCode::STORAGE_FAILURE);

  assert(persistence.erase(key).ok());
  assert(persistence.load(key, output, sizeof(output), bytesRead).code ==
         ErrorCode::NOT_FOUND);
  return 0;
}
