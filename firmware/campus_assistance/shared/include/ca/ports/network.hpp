#pragma once

#include "ca/types.hpp"

namespace ca {

// Provisioned station-side recipient roster for reliable request broadcasts.
// The registry is deliberately independent of MCU and radio implementation.
class ICartRegistry {
public:
  virtual ~ICartRegistry() {}
  virtual uint16_t enabledCount() const = 0;
  virtual DeviceId enabledCartAt(uint16_t index) const = 0;
};

} // namespace ca
