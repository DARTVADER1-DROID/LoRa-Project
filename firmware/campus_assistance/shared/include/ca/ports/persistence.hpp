#pragma once

#include "ca/types.hpp"

namespace ca {

class IPersistence {
public:
  virtual ~IPersistence() {}

  // On success, bytesRead is set to the exact record length. BUFFER_FULL
  // reports the required length in bytesRead without writing past capacity.
  virtual Status
  load(RecordKey key, uint8_t* output, uint16_t capacity, uint16_t& bytesRead) = 0;

  // REQUIRED: after reset/power loss, load() must expose either the complete
  // previous value or the complete new value, never a torn mixture. Adapters
  // must implement this with a journal/dual-slot/atomic page strategy suitable
  // for their medium and document endurance and erase constraints.
  virtual Status
  storeAtomically(RecordKey key, const uint8_t* data, uint16_t length) = 0;
  virtual Status erase(RecordKey key) = 0;
};

} // namespace ca
