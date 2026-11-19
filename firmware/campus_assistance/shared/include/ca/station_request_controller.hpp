#pragma once

#include "ca/domain/destination_catalog.hpp"
#include "ca/domain/request_lifecycle.hpp"
#include "protocol.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

// Station request use case: selection, submission and cancellation.
// It coordinates abstractions but does not implement persistence, arbitration,
// frame encoding or hardware access.
class StationRequestController {
public:
  StationRequestController(DestinationCatalog& destinations,
                           RequestLifecycle& lifecycle,
                           ProtocolEndpoint& protocol,
                           const DeviceConfig& config);

  Status selectDestination(DeviceId destinationId);
  Status submit(RequestId& createdRequestId);
  Status cancelCurrentRequest();
  bool hasCurrentRequest() const;

private:
  DestinationCatalog& destinations_;
  RequestLifecycle& lifecycle_;
  ProtocolEndpoint& protocol_;
  DeviceConfig config_;
  DeviceId selectedDestinationId_;
  RequestId currentRequestId_;
  bool hasSelection_;
  bool hasCurrentRequest_;
};

} // namespace ca
