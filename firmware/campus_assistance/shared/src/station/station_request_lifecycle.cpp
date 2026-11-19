#include "ca/station/station_request_lifecycle.hpp"

namespace ca {

StationRequestLifecycle::StationRequestLifecycle(IRequestRepository& repository,
                                                 const DestinationCatalog& destinations,
                                                 const IClock& clock,
                                                 DeviceId stationId,
                                                 uint32_t requestLifetimeMs)
    : repository_(repository), destinations_(destinations), clock_(clock),
      stationId_(stationId), requestLifetimeMs_(requestLifetimeMs), configured_(false) {
}

Status StationRequestLifecycle::configure() {
  if (stationId_ == 0 || requestLifetimeMs_ == 0 || requestLifetimeMs_ > 0x7FFFFFFFUL)
    return Status(ErrorCode::INVALID_ARGUMENT);

  configured_ = true;
  return Status();
}

Status StationRequestLifecycle::create(const NewRequest& input,
                                       RequestRecord& created) {
  if (!configured_)
    return Status(ErrorCode::INVALID_STATE);
  if (input.destinationId == 0 || !destinations_.isEnabled(input.destinationId))
    return Status(ErrorCode::INVALID_ARGUMENT);

  Destination destination = {};
  if (!destinations_.findById(input.destinationId, destination) || !destination.enabled)
    return Status(ErrorCode::INVALID_ARGUMENT);

  RequestId id = {};
  const Status allocation = repository_.reserveNextRequestId(stationId_, id);
  if (!allocation.ok())
    return allocation;

  const uint32_t now = clock_.millisNow();
  RequestRecord record = {};
  record.id = id;
  record.destinationId = input.destinationId;
  record.createdAtMs = now;
  record.expiresAtMs = now + requestLifetimeMs_;
  record.state = RequestState::PENDING_UNASSIGNED;
  record.claimState = ClaimState::IDLE;

  const Status insertion = repository_.insertNew(record);
  if (!insertion.ok())
    return insertion;

  created = record;
  return Status();
}

Status StationRequestLifecycle::cancel(const RequestId& id, CancellationSource source) {
  if (!configured_)
    return Status(ErrorCode::INVALID_STATE);

  RequestRecord updated = {};
  return repository_.cancelIfUnassigned(id, source, updated);
}

Status StationRequestLifecycle::expireDue(uint32_t stationNowMs,
                                          ExpiredRequestSet& expired) {
  if (!configured_)
    return Status(ErrorCode::INVALID_STATE);
  expired.count = 0;
  if (expired.capacity > 0 && expired.items == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);

  uint16_t dueCount = 0;
  for (uint16_t i = 0; i < repository_.size(); ++i) {
    RequestRecord record = {};
    if (!repository_.recordAt(i, record))
      return Status(ErrorCode::STORAGE_FAILURE);
    if (!isExpiryEligible(record.state))
      continue;
    const uint32_t lifetime =
        static_cast<uint32_t>(record.expiresAtMs - record.createdAtMs);
    if (hasElapsed(stationNowMs, record.createdAtMs, lifetime))
      ++dueCount;
  }

  if (dueCount > expired.capacity) {
    expired.count = dueCount;
    return Status(ErrorCode::BUFFER_FULL);
  }

  uint16_t written = 0;
  for (uint16_t i = 0; i < repository_.size(); ++i) {
    RequestRecord record = {};
    if (!repository_.recordAt(i, record))
      return Status(ErrorCode::STORAGE_FAILURE);
    if (!isExpiryEligible(record.state))
      continue;

    const uint32_t lifetime =
        static_cast<uint32_t>(record.expiresAtMs - record.createdAtMs);
    if (!hasElapsed(stationNowMs, record.createdAtMs, lifetime))
      continue;

    RequestRecord updated = {};
    const Status result = repository_.expireIfDue(record.id, stationNowMs, updated);
    if (!result.ok()) {
      expired.count = written;
      return result;
    }
    expired.items[written++] = record.id;
  }

  expired.count = written;
  return Status();
}

Status StationRequestLifecycle::applyJourneyUpdate(const JourneyUpdate& update) {
  if (!configured_)
    return Status(ErrorCode::INVALID_STATE);
  RequestRecord updated = {};
  return repository_.applyJourneyUpdateIfCurrent(update, updated);
}

bool StationRequestLifecycle::find(const RequestId& id, RequestRecord& record) const {
  return configured_ && repository_.find(id, record);
}

bool StationRequestLifecycle::isExpiryEligible(RequestState state) const {
  return state == RequestState::PENDING_UNASSIGNED ||
         state == RequestState::CLAIM_PENDING;
}

} // namespace ca
