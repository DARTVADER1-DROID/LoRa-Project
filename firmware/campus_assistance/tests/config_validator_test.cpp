#include "CampusAssistance.h"

#include <assert.h>
#include <string.h>

using namespace ca;

namespace {
DefaultConfigValidator validator;

DeviceConfig validDeviceConfig() {
  DeviceConfig config = {};
  config.role = DeviceRole::STATION;
  config.deviceId = 1;
  config.networkId = 10;
  config.protocolVersion = 1;
  config.radioProfileId = 1;
  config.boardProfileId = 1;
  config.pendingCapacity = 1;
  config.requestLifetimeMs = kDefaultRequestLifetimeMs;
  return config;
}

RadioProfile validRadioProfile() {
  RadioProfile profile = {};
  profile.frequencyHz = 433000000UL;
  profile.spreadingFactor = 7;
  profile.bandwidthHz = 125000UL;
  profile.codingRate = 1;
  profile.transmitPowerDbm = 17;
  return profile;
}

void testDeviceConfig() {
  DeviceConfig config = validDeviceConfig();
  assert(validator.validate(config).ok());
  config.deviceId = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.networkId = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.protocolVersion = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.radioProfileId = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.boardProfileId = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.pendingCapacity = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.requestLifetimeMs = 0;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.requestLifetimeMs = 0x80000000UL;
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
  config = validDeviceConfig();
  config.role = static_cast<DeviceRole>(0xFF);
  assert(validator.validate(config).code == ErrorCode::CONFIGURATION_INVALID);
}

void testStationRegistry() {
  StationRegistryEntry entries[2] = {};
  entries[0].stationId = 1;
  entries[1].stationId = 2;
  StationRegistry registry = {2, entries, 2};
  assert(validator.validateStationRegistry(registry).ok());
  registry.count = 0;
  registry.entries = 0;
  registry.capacity = 0;
  assert(validator.validateStationRegistry(registry).ok());
  registry.count = 3;
  registry.entries = entries;
  registry.capacity = 2;
  assert(validator.validateStationRegistry(registry).code ==
         ErrorCode::CONFIGURATION_INVALID);
  registry.count = 1;
  registry.entries = 0;
  assert(validator.validateStationRegistry(registry).code ==
         ErrorCode::CONFIGURATION_INVALID);
  registry.entries = entries;
  entries[1].stationId = entries[0].stationId;
  registry.count = 2;
  assert(validator.validateStationRegistry(registry).code ==
         ErrorCode::CONFIGURATION_INVALID);
  entries[0].stationId = 0;
  assert(validator.validateStationRegistry(registry).code ==
         ErrorCode::CONFIGURATION_INVALID);
}

void testDestinationConfig() {
  Destination destinations[2] = {};
  destinations[0].id = 10;
  strcpy(destinations[0].label, "Library");
  destinations[1].id = 20;
  strcpy(destinations[1].label, "Canteen");
  DestinationConfig config = {2, destinations, 2};
  assert(validator.validateDestinationConfig(config).ok());
  config.count = 0;
  config.entries = 0;
  config.capacity = 0;
  assert(validator.validateDestinationConfig(config).ok());
  config.count = 3;
  config.entries = destinations;
  config.capacity = 2;
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
  config.count = 1;
  config.entries = 0;
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
  config.entries = destinations;
  destinations[0].id = 0;
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
  destinations[0].id = 10;
  destinations[0].label[0] = '\0';
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
  memset(destinations[0].label, 'X', kMaxDestinationLabelBytes);
  destinations[0].label[kMaxDestinationLabelBytes] = '\0';
  assert(validator.validateDestinationConfig(config).ok());
  memset(destinations[0].label, 'X', sizeof(destinations[0].label));
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
  strcpy(destinations[0].label, "Library");
  destinations[1].id = destinations[0].id;
  config.count = 2;
  assert(validator.validateDestinationConfig(config).code ==
         ErrorCode::CONFIGURATION_INVALID);
}

void testRadioProfile() {
  RadioProfile profile = validRadioProfile();
  assert(validator.validateRadioProfile(profile).ok());
  profile.frequencyHz = 409000000UL;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.frequencyHz = 525000000UL;
  assert(validator.validateRadioProfile(profile).ok());
  profile = validRadioProfile();
  profile.spreadingFactor = 5;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.spreadingFactor = 12;
  assert(validator.validateRadioProfile(profile).ok());
  profile = validRadioProfile();
  profile.bandwidthHz = 100000UL;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.bandwidthHz = 500000UL;
  assert(validator.validateRadioProfile(profile).ok());
  profile = validRadioProfile();
  profile.codingRate = 5;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.transmitPowerDbm = 1;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.transmitPowerDbm = 18;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
  profile = validRadioProfile();
  profile.transmitPowerDbm = 21;
  assert(validator.validateRadioProfile(profile).code ==
         ErrorCode::CONFIGURATION_INVALID);
}
} // namespace

int main() {
  testDeviceConfig();
  testStationRegistry();
  testDestinationConfig();
  testRadioProfile();
  return 0;
}
