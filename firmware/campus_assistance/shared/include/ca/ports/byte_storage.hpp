#pragma once

#include "ca/types.hpp"

namespace ca {

// Random-access bounded byte store. write() processes bytes in ascending offset
// order; sync() makes prior writes durable. A failed sync must leave the previous
// durable image intact. FixedDualSlotPersistence relies on this ordering to
// program the slot's commit marker last.
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
