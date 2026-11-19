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

// Portable default checks for the RA-02/SX1278 configuration surface.
// Regulatory approval and board-specific compatibility remain separate gates.
class DefaultConfigValidator final : public ConfigValidator {
public:
  Status validate(const DeviceConfig& config) const override;
  Status validateStationRegistry(const StationRegistry& registry) const override;
  Status
  validateDestinationConfig(const DestinationConfig& destinations) const override;
  Status validateRadioProfile(const RadioProfile& profile) const override;
};

} // namespace ca
