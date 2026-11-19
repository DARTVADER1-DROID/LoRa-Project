#include "ca/config.hpp"

#include <limits.h>

namespace ca {
namespace {

Status invalidConfiguration() {
  return Status(ErrorCode::CONFIGURATION_INVALID);
}

bool isValidRole(DeviceRole role) {
  switch (role) {
  case DeviceRole::STATION:
  case DeviceRole::CART:
    return true;
  }
  return false;
}

bool isSupportedBandwidth(uint32_t bandwidthHz) {
  switch (bandwidthHz) {
  case 7800UL:
  case 10400UL:
  case 15600UL:
  case 20800UL:
  case 31250UL:
  case 41700UL:
  case 62500UL:
  case 125000UL:
  case 250000UL:
  case 500000UL:
    return true;
  default:
    return false;
  }
}

uint8_t boundedStringLength(const char* value, uint8_t capacity) {
  uint8_t length = 0;
  while (length < capacity && value[length] != '\0')
    ++length;
  return length;
}

} // namespace

Status DefaultConfigValidator::validate(const DeviceConfig& config) const {
  if (!isValidRole(config.role) || config.deviceId == 0 || config.networkId == 0 ||
      config.protocolVersion == 0 || config.radioProfileId == 0 ||
      config.boardProfileId == 0 || config.pendingCapacity == 0 ||
      config.requestLifetimeMs == 0 ||
      config.requestLifetimeMs > static_cast<uint32_t>(INT32_MAX)) {
    return invalidConfiguration();
  }

  return Status();
}

Status
DefaultConfigValidator::validateStationRegistry(const StationRegistry& registry) const {
  if (registry.count > registry.capacity ||
      (registry.count > 0 && registry.entries == 0)) {
    return invalidConfiguration();
  }

  for (uint16_t i = 0; i < registry.count; ++i) {
    if (registry.entries[i].stationId == 0)
      return invalidConfiguration();

    for (uint16_t j = 0; j < i; ++j) {
      if (registry.entries[i].stationId == registry.entries[j].stationId)
        return invalidConfiguration();
    }
  }

  return Status();
}

Status DefaultConfigValidator::validateDestinationConfig(
    const DestinationConfig& destinations) const {
  if (destinations.count > destinations.capacity ||
      (destinations.count > 0 && destinations.entries == 0)) {
    return invalidConfiguration();
  }

  for (uint16_t i = 0; i < destinations.count; ++i) {
    const Destination& destination = destinations.entries[i];
    const uint8_t labelLength =
        boundedStringLength(destination.label, kMaxDestinationLabelBytes + 1);

    if (destination.id == 0 || labelLength == 0 ||
        labelLength > kMaxDestinationLabelBytes) {
      return invalidConfiguration();
    }

    for (uint16_t j = 0; j < i; ++j) {
      if (destination.id == destinations.entries[j].id)
        return invalidConfiguration();
    }
  }

  return Status();
}

Status DefaultConfigValidator::validateRadioProfile(const RadioProfile& profile) const {
  // RA-02/SX1278 module tuning range; this is not a regulatory authorization.
  if (profile.frequencyHz < 410000000UL || profile.frequencyHz > 525000000UL ||
      profile.spreadingFactor < 6 || profile.spreadingFactor > 12 ||
      !isSupportedBandwidth(profile.bandwidthHz) || profile.codingRate < 1 ||
      profile.codingRate > 4 || profile.transmitPowerDbm < 2 ||
      profile.transmitPowerDbm > 17) {
    return invalidConfiguration();
  }

  return Status();
}

} // namespace ca
