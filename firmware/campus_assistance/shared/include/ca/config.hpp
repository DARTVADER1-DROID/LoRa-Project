#pragma once
#include "types.hpp"
namespace ca {
struct StationRegistry;
class ConfigValidator {
public:
  Status validate(const DeviceConfig& config) const;
  Status validateStationRegistry(const StationRegistry& registry) const;
  Status validateRadioProfile(const RadioProfile& profile) const;
};
class StationRequestController {
public:
  Status selectDestination(DeviceId destinationId);
  Status submit(RequestId& createdRequestId);
  Status cancelCurrentRequest();
};
}
