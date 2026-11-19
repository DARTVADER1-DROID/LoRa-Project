#pragma once

#include "domain.hpp"
#include "protocol.hpp"
#include "ca/ports/runtime.hpp"

namespace ca {

// Station request use case: selection, submission and cancellation.
// It coordinates abstractions but does not implement persistence, arbitration,
// frame encoding or hardware access.
class StationRequestController {
public:
  StationRequestController(DestinationCatalog& destinations,
                           RequestLifecycle& lifecycle,
                           IClock& clock,
                           ProtocolEndpoint& protocol);

  Status selectDestination(DeviceId destinationId);
  Status submit(RequestId& createdRequestId);
  Status cancelCurrentRequest();

private:
  DestinationCatalog& destinations_;
  RequestLifecycle& lifecycle_;
  IClock& clock_;
  ProtocolEndpoint& protocol_;
  DeviceId selectedDestinationId_;
  RequestId currentRequestId_;
  bool hasSelection_;
  bool hasCurrentRequest_;
};

} // namespace ca
