#pragma once

#include "ca/domain.hpp"
#include "ca/ports/request_repository.hpp"

namespace ca {

// Bounded, allocation-free request ledger for a single-threaded station event
// loop and host simulation. This is deliberately volatile: do not use it as
// the production persistence layer until an atomic durable journal is added.
template <uint16_t StorageCapacity>
class FixedRequestRepository final : public IRequestRepository {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedRequestRepository() : count_(0), configured_(false) {}

  Status configure() {
    if (configured_)
      return count_ == 0 ? Status() : Status(ErrorCode::INVALID_STATE);
    count_ = 0;
    configured_ = true;
    return Status();
  }

  Status insertNew(const RequestRecord& record) override {
    if (!configured_)
      return Status(ErrorCode::INVALID_STATE);
    if (!RequestIdentity::isValid(record.id) || record.destinationId == 0 ||
        record.state != RequestState::PENDING_UNASSIGNED ||
        record.claimState != ClaimState::IDLE ||
        record.expiresAtMs == record.createdAtMs ||
        static_cast<uint32_t>(record.expiresAtMs - record.createdAtMs) > 0x7FFFFFFFUL)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (findIndex(record.id) >= 0)
      return Status(ErrorCode::DUPLICATE);
    if (count_ >= StorageCapacity)
      return Status(ErrorCode::CAPACITY_EXCEEDED);
    records_[count_++] = record;
    return Status();
  }

  bool find(const RequestId& id, RequestRecord& record) const override {
    const int16_t index = findIndex(id);
    if (index < 0)
      return false;
    record = records_[index];
    return true;
  }

  Status beginClaimIfEligible(const RequestId& id,
                              DeviceId cartId,
                              TransactionId transactionId,
                              uint32_t now,
                              RequestRecord& updated) override {
    RequestRecord* record = mutableFind(id);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (cartId == 0 || transactionId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (record->state == RequestState::CANCELLED)
      return Status(ErrorCode::CANCELLED);
    if (record->state == RequestState::EXPIRED)
      return Status(ErrorCode::EXPIRED);
    if (isAssignedState(record->state))
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (due(*record, now)) {
      expire(*record);
      updated = *record;
      return Status(ErrorCode::EXPIRED);
    }
    if (record->state != RequestState::PENDING_UNASSIGNED ||
        (record->claimState != ClaimState::IDLE &&
         record->claimState != ClaimState::WITHDRAWN))
      return Status(ErrorCode::BUSY);
    record->claimState = ClaimState::PENDING;
    record->claimCartId = cartId;
    record->claimTransactionId = transactionId;
    record->state = RequestState::CLAIM_PENDING;
    updated = *record;
    return Status();
  }

  Status withdrawClaimIfPending(const RequestId& id,
                                DeviceId cartId,
                                TransactionId transactionId,
                                RequestRecord& updated) override {
    RequestRecord* record = mutableFind(id);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (record->claimState == ClaimState::COMMITTED)
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (record->claimState == ClaimState::WITHDRAWN && record->claimCartId == cartId &&
        record->claimTransactionId == transactionId) {
      updated = *record;
      return Status();
    }
    if (record->claimState != ClaimState::PENDING || record->claimCartId != cartId ||
        record->claimTransactionId != transactionId)
      return Status(ErrorCode::STALE_EVENT);
    record->claimState = ClaimState::WITHDRAWN;
    record->state = RequestState::PENDING_UNASSIGNED;
    updated = *record;
    return Status();
  }

  Status commitAssignmentIfClaimPending(const RequestId& id,
                                        DeviceId cartId,
                                        TransactionId transactionId,
                                        AssignmentEpoch epoch,
                                        uint32_t now,
                                        RequestRecord& updated) override {
    RequestRecord* record = mutableFind(id);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (record->claimState == ClaimState::COMMITTED && record->claimCartId == cartId &&
        record->claimTransactionId == transactionId) {
      if (record->assignmentEpoch != epoch)
        return Status(ErrorCode::STALE_EVENT);
      updated = *record;
      return Status();
    }
    if (record->state == RequestState::CANCELLED)
      return Status(ErrorCode::CANCELLED);
    if (record->state == RequestState::EXPIRED)
      return Status(ErrorCode::EXPIRED);
    if (isAssignedState(record->state))
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (due(*record, now)) {
      expire(*record);
      updated = *record;
      return Status(ErrorCode::EXPIRED);
    }
    if (epoch == 0 || record->claimState != ClaimState::PENDING ||
        record->claimCartId != cartId || record->claimTransactionId != transactionId ||
        record->state != RequestState::CLAIM_PENDING)
      return Status(ErrorCode::STALE_EVENT);
    record->claimState = ClaimState::COMMITTED;
    record->state = RequestState::ASSIGNED;
    record->assignedCartId = cartId;
    record->assignmentEpoch = epoch;
    updated = *record;
    return Status();
  }

  Status cancelIfUnassigned(const RequestId& id,
                            CancellationSource source,
                            RequestRecord& updated) override {
    switch (source) {
    case CancellationSource::STUDENT:
    case CancellationSource::STATION_OPERATOR:
    case CancellationSource::SYSTEM:
      break;
    default:
      return Status(ErrorCode::INVALID_ARGUMENT);
    }

    RequestRecord* record = mutableFind(id);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (isAssignedState(record->state))
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (record->state == RequestState::CANCELLED) {
      updated = *record;
      return Status();
    }
    if (record->state == RequestState::EXPIRED)
      return Status(ErrorCode::EXPIRED);
    record->state = RequestState::CANCELLED;
    if (record->claimState == ClaimState::PENDING)
      record->claimState = ClaimState::WITHDRAWN;
    updated = *record;
    return Status();
  }

  Status
  expireIfDue(const RequestId& id, uint32_t now, RequestRecord& updated) override {
    RequestRecord* record = mutableFind(id);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (isAssignedState(record->state))
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (record->state == RequestState::CANCELLED)
      return Status(ErrorCode::CANCELLED);
    if (record->state == RequestState::EXPIRED) {
      updated = *record;
      return Status();
    }
    if (!due(*record, now))
      return Status(ErrorCode::INVALID_STATE);
    expire(*record);
    updated = *record;
    return Status();
  }

  Status applyJourneyUpdateIfCurrent(const JourneyUpdate& update,
                                     RequestRecord& updated) override {
    RequestRecord* record = mutableFind(update.requestId);
    if (!record)
      return Status(ErrorCode::NOT_FOUND);
    if (!isValidJourneyStatus(update.status) || update.assignedCartId == 0 ||
        record->assignedCartId != update.assignedCartId ||
        (record->state != RequestState::ASSIGNED &&
         record->state != RequestState::ON_THE_WAY &&
         record->state != RequestState::ARRIVED_AT_PICKUP))
      return Status(ErrorCode::STALE_EVENT);
    if (compareSequence(update.eventSequence, record->lastJourneyEventSequence) !=
        SerialNumberOrder::NEWER)
      return Status(ErrorCode::STALE_EVENT);
    if (!validJourneyStep(record->state, update.status))
      return Status(ErrorCode::INVALID_STATE);
    record->lastJourneyEventSequence = update.eventSequence;
    record->state = toRequestState(update.status);
    updated = *record;
    return Status();
  }

  Status removeTerminalBefore(uint32_t) override {
    // Time-based retention requires a persisted monotonic-age policy. Until
    // that policy exists, never discard terminal history implicitly.
    return Status(ErrorCode::NOT_IMPLEMENTED);
  }

  uint16_t size() const override { return count_; }

private:
  RequestRecord records_[StorageCapacity];
  uint16_t count_;
  bool configured_;

  int16_t findIndex(const RequestId& id) const {
    for (uint16_t i = 0; i < count_; ++i)
      if (RequestIdentity::equals(records_[i].id, id))
        return static_cast<int16_t>(i);
    return -1;
  }

  RequestRecord* mutableFind(const RequestId& id) {
    const int16_t index = findIndex(id);
    return index < 0 ? 0 : &records_[index];
  }

  static bool isAssignedState(RequestState state) {
    return state == RequestState::ASSIGNED || state == RequestState::ON_THE_WAY ||
           state == RequestState::ARRIVED_AT_PICKUP || state == RequestState::COMPLETED;
  }

  static bool due(const RequestRecord& record, uint32_t now) {
    return static_cast<uint32_t>(now - record.createdAtMs) >=
           static_cast<uint32_t>(record.expiresAtMs - record.createdAtMs);
  }

  static void expire(RequestRecord& record) {
    record.state = RequestState::EXPIRED;
    if (record.claimState == ClaimState::PENDING)
      record.claimState = ClaimState::WITHDRAWN;
  }

  static bool validJourneyStep(RequestState current, JourneyStatus next) {
    return (current == RequestState::ASSIGNED && next == JourneyStatus::ON_THE_WAY) ||
           (current == RequestState::ON_THE_WAY &&
            next == JourneyStatus::ARRIVED_AT_PICKUP) ||
           (current == RequestState::ARRIVED_AT_PICKUP &&
            next == JourneyStatus::COMPLETED);
  }

  static RequestState toRequestState(JourneyStatus status) {
    return status == JourneyStatus::ON_THE_WAY
               ? RequestState::ON_THE_WAY
               : (status == JourneyStatus::ARRIVED_AT_PICKUP
                      ? RequestState::ARRIVED_AT_PICKUP
                      : RequestState::COMPLETED);
  }
};

} // namespace ca
