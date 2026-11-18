#include "../include/ca/config.hpp"
namespace ca {
Status ConfigValidator::validate(const DeviceConfig& config) const {
  // TODO: validate role, unique provisioned identity, nonzero capacities and supported protocol/profile.
  (void)config;
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
Status ConfigValidator::validateStationRegistry(const StationRegistry& registry) const {
  // TODO: reject duplicate station/destination IDs and invalid registry entries.
  (void)registry;
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
Status ConfigValidator::validateRadioProfile(const RadioProfile& profile) const {
  // TODO: validate only against the exact RA-02 variant and legally approved RF configuration.
  (void)profile;
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
Status StationRequestController::selectDestination(DeviceId destinationId) {
  // TODO: select only a registered, enabled destination.
  (void)destinationId;
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
Status StationRequestController::submit(RequestId& createdRequestId) {
  // TODO: create unique station-owned request, start 600000 ms default lifetime and queue REQUEST.
  (void)createdRequestId;
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
Status StationRequestController::cancelCurrentRequest() {
  // TODO: issue station-authoritative cancellation for the current full RequestId.
  return Status(ErrorCode::NOT_IMPLEMENTED);
}
}
