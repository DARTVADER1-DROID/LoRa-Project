#pragma once

#include "ca/domain/request_lifecycle.hpp"
#include "ca/domain/destination_catalog.hpp"
#include "ca/ports/request_repository.hpp"
#include "ca/ports/runtime.hpp"

namespace ca {

// Station use-case implementation. It owns request creation policy and expiry
// scheduling, while the repository owns durable identity allocation and all
// conditional state transitions.
class StationRequestLifecycle final : public RequestLifecycle {
public:
  StationRequestLifecycle(IRequestRepository& repository,
                          const DestinationCatalog& destinations,
                          const IClock& clock,
                          DeviceId stationId,
                          uint32_t requestLifetimeMs,
                          uint32_t terminalRetentionMs = kDefaultTerminalRetentionMs);

  Status configure() override;
  Status create(const NewRequest& input, RequestRecord& created) override;
  Status cancel(const RequestId& id, CancellationSource source) override;
  Status expireDue(uint32_t stationNowMs, ExpiredRequestSet& expired) override;
  Status applyJourneyUpdate(const JourneyUpdate& update) override;
  bool find(const RequestId& id, RequestRecord& record) const override;

private:
  IRequestRepository& repository_;
  const DestinationCatalog& destinations_;
  const IClock& clock_;
  DeviceId stationId_;
  uint32_t requestLifetimeMs_;
  bool configured_;

  bool isExpiryEligible(RequestState state) const;
};

} // namespace ca
