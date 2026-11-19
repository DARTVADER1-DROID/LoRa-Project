#pragma once

#include "types.hpp"

namespace ca {

// Configuration validation only. Request submission belongs to the station
// application/use-case layer and is deliberately not coupled to this service.
class ConfigValidator {
public:
  virtual ~ConfigValidator() {}
  virtual Status validate(const DeviceConfig& config) const = 0;
  virtual Status validateStationRegistry(const StationRegistry& registry) const = 0;
  virtual Status validateRadioProfile(const RadioProfile& profile) const = 0;
};

} // namespace ca
