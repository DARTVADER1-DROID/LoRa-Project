#pragma once

#include "ca/types.hpp"

namespace ca {

// Provisioned station-side recipient roster for reliable request broadcasts.
// The registry is deliberately independent of MCU and radio implementation.
class IStationRegistry {
public:
  virtual ~IStationRegistry() {}
  virtual bool containsStation(DeviceId stationId) const = 0;
};

class ICartRegistry {
public:
  virtual ~ICartRegistry() {}
  virtual uint16_t enabledCount() const = 0;
  virtual DeviceId enabledCartAt(uint16_t index) const = 0;
  virtual bool containsCart(DeviceId cartId) const = 0;
};

} // namespace ca
