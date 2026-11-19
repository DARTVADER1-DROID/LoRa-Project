#pragma once

#include "types.hpp"

namespace ca {

// Provisioned configuration is supplied as bounded, non-owning views.
class ConfigValidator {
public:
  virtual ~ConfigValidator() {}
  virtual Status validate(const DeviceConfig& config) const = 0;
  virtual Status validateStationRegistry(const StationRegistry& registry) const = 0;
  virtual Status
  validateDestinationConfig(const DestinationConfig& destinations) const = 0;
  virtual Status validateRadioProfile(const RadioProfile& profile) const = 0;
};

} // namespace ca
