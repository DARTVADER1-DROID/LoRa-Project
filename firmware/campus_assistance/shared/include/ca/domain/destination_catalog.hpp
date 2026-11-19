#pragma once

#include "ca/types.hpp"

namespace ca {

class DestinationCatalog {
public:
  virtual ~DestinationCatalog() {}
  virtual Status load(const DestinationConfig& config) = 0;
  virtual uint16_t count() const = 0;
  virtual bool getByIndex(uint16_t index, Destination& destination) const = 0;
  virtual bool findById(DeviceId id, Destination& destination) const = 0;
  virtual bool isEnabled(DeviceId id) const = 0;
};

} // namespace ca
