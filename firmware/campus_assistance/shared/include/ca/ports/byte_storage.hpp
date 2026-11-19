#pragma once

#include "ca/types.hpp"

namespace ca {

// Random-access bounded byte store. sync() must make prior writes durable;
// FixedDualSlotPersistence provides old-or-new record visibility above it.
class IByteStorage {
public:
  virtual ~IByteStorage() {}
  virtual Status begin() = 0;
  virtual uint16_t capacity() const = 0;
  virtual Status read(uint16_t offset, uint8_t* output, uint16_t length) const = 0;
  virtual Status write(uint16_t offset, const uint8_t* data, uint16_t length) = 0;
  virtual Status sync() = 0;
};

} // namespace ca
