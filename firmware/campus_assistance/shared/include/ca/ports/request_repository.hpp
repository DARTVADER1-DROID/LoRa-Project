#pragma once

#include "ca/types.hpp"

namespace ca {

// Port for authoritative station-side request storage. Implementations must
// serialize each conditional transition against all competing transitions for
// the same complete RequestId. Persistent implementations must durably reserve
// RequestIds before returning them and commit each mutation before reporting
// success. recordAt() is a bounded read-only snapshot read for one event loop.
class IRequestRepository {
public:
  virtual ~IRequestRepository() {}

  virtual Status reserveNextRequestId(DeviceId stationId, RequestId& id) = 0;
  virtual Status insertNew(const RequestRecord& record) = 0;
  virtual bool find(const RequestId& id, RequestRecord& record) const = 0;
  virtual bool recordAt(uint16_t index, RequestRecord& record) const = 0;
  virtual Status beginClaimIfEligible(const RequestId& id,
                                      DeviceId cartId,
                                      TransactionId transactionId,
                                      uint32_t stationNowMs,
                                      RequestRecord& updated) = 0;
  virtual Status withdrawClaimIfPending(const RequestId& id,
                                        DeviceId cartId,
                                        TransactionId transactionId,
                                        RequestRecord& updated) = 0;
  virtual Status commitAssignmentIfClaimPending(const RequestId& id,
                                                DeviceId cartId,
                                                TransactionId transactionId,
                                                AssignmentEpoch epoch,
                                                uint32_t stationNowMs,
                                                RequestRecord& updated) = 0;
  virtual Status cancelIfUnassigned(const RequestId& id,
                                    CancellationSource source,
                                    RequestRecord& updated) = 0;
  virtual Status
  expireIfDue(const RequestId& id, uint32_t stationNowMs, RequestRecord& updated) = 0;
  virtual Status applyJourneyUpdateIfCurrent(const JourneyUpdate& update,
                                             RequestRecord& updated) = 0;
  virtual Status removeTerminalBefore(uint32_t retentionBoundaryMs) = 0;
  virtual uint16_t size() const = 0;
};

} // namespace ca
