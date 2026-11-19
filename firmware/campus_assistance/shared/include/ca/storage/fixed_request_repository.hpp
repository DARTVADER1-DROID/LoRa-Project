#pragma once

#include "ca/domain/request_identity.hpp"
#include "ca/ports/request_repository.hpp"

namespace ca {

// Bounded, allocation-free request ledger for a single-threaded station event
// loop and host simulation. This is deliberately volatile: do not use it as
// the production persistence layer until an atomic durable journal is added.
template <uint16_t StorageCapacity>
class FixedRequestRepository final : public IRequestRepository {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedRequestRepository()
      : count_(0), configured_(false), nextSequence_(1), nextGeneration_(1),
        identifiersExhausted_(false) {}

  Status configure() {
    if (configured_)
      return count_ == 0 ? Status() : Status(ErrorCode::INVALID_STATE);
    count_ = 0;
    nextSequence_ = 1;
    nextGeneration_ = 1;
    identifiersExhausted_ = false;
    configured_ = true;
    return Status();
  }

  Status reserveNextRequestId(DeviceId stationId, RequestId& id) override {
    if (!configured_)
      return Status(ErrorCode::INVALID_STATE);
    if (stationId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (identifiersExhausted_)
      return Status(ErrorCode::CAPACITY_EXCEEDED);

    RequestId reserved = {};
    reserved.originStationId = stationId;
    reserved.sequence = nextSequence_;
    reserved.generation = nextGeneration_;

    if (nextSequence_ == 0xFFFFFFFFUL) {
      if (nextGeneration_ == 0xFFFFU) {
        identifiersExhausted_ = true;
      } else {
        nextSequence_ = 0;
        ++nextGeneration_;
      }
    } else {
      ++nextSequence_;
    }
    id = reserved;
    return Status();
  }

  Status insertNew(const RequestRecord& record) override {
    if (!configured_)
      return Status(ErrorCode::INVALID_STATE);
    if (!RequestIdentity::isValid(record.id) || record.destinationId == 0 ||
        record.state != RequestState::PENDING_UNASSIGNED ||
        record.claimState != ClaimState::IDLE || record.assignedCartId != 0 ||
        record.assignmentEpoch != 0 || record.claimCartId != 0 ||
        record.claimTransactionId != 0 || record.lastJourneyEventSequence != 0 ||
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

  bool recordAt(uint16_t index, RequestRecord& record) const override {
    if (index >= count_)
      return false;
    record = records_[index];
    return true;
  }

  // Recovery-only insertion. It validates the decoded record but does not
  // require the initial PENDING_UNASSIGNED state used by insertNew().
  Status restoreRecord(const RequestRecord& record) {
    if (!configured_ || !RequestIdentity::isValid(record.id) ||
        record.destinationId == 0 || record.expiresAtMs == record.createdAtMs ||
        static_cast<uint32_t>(record.expiresAtMs - record.createdAtMs) > 0x7FFFFFFFUL ||
        static_cast<uint8_t>(record.state) >
            static_cast<uint8_t>(RequestState::COMMUNICATION_UNCERTAIN) ||
        static_cast<uint8_t>(record.claimState) >
            static_cast<uint8_t>(ClaimState::COMMITTED) ||
        record.state == RequestState::CREATED ||
        record.state == RequestState::COMMUNICATION_UNCERTAIN)
      return Status(ErrorCode::INVALID_ARGUMENT);
    const bool assigned = record.state == RequestState::ASSIGNED ||
                          record.state == RequestState::ON_THE_WAY ||
                          record.state == RequestState::ARRIVED_AT_PICKUP ||
                          record.state == RequestState::COMPLETED;
    if (assigned && (record.assignedCartId == 0 || record.assignmentEpoch == 0 ||
                     record.claimState != ClaimState::COMMITTED))
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (record.state == RequestState::CLAIM_PENDING &&
        (record.claimState != ClaimState::PENDING || record.claimCartId == 0 ||
         record.claimTransactionId == 0))
      return Status(ErrorCode::INVALID_ARGUMENT);
    if ((record.state == RequestState::PENDING_UNASSIGNED ||
         record.state == RequestState::CANCELLED ||
         record.state == RequestState::EXPIRED) &&
        (record.assignedCartId != 0 || record.assignmentEpoch != 0 ||
         record.claimState == ClaimState::PENDING ||
         record.claimState == ClaimState::COMMITTED))
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (findIndex(record.id) >= 0)
      return Status(ErrorCode::DUPLICATE);
    if (count_ >= StorageCapacity)
      return Status(ErrorCode::CAPACITY_EXCEEDED);
    records_[count_++] = record;
    return Status();
  }

  // Without a persistent wall clock, elapsed millis cannot be trusted across
  // a station reboot. Fail closed by expiring only unassigned requests; an
  // already committed ride remains assigned and recoverable.
  Status expireUnassignedForRecovery() {
    if (!configured_)
      return Status(ErrorCode::INVALID_STATE);
    for (uint16_t i = 0; i < count_; ++i) {
      if (records_[i].state == RequestState::PENDING_UNASSIGNED ||
          records_[i].state == RequestState::CLAIM_PENDING) {
        records_[i].state = RequestState::EXPIRED;
        if (records_[i].claimState == ClaimState::PENDING)
          records_[i].claimState = ClaimState::WITHDRAWN;
      }
    }
    return Status();
  }

  uint32_t nextSequence() const { return nextSequence_; }
  uint16_t nextGeneration() const { return nextGeneration_; }
  bool identifiersExhausted() const { return identifiersExhausted_; }

  Status restoreAllocatorState(uint32_t sequence, uint16_t generation, bool exhausted) {
    if (!configured_)
      return Status(ErrorCode::INVALID_STATE);
    nextSequence_ = sequence;
    nextGeneration_ = generation;
    identifiersExhausted_ = exhausted;
    return Status();
  }

private:
  RequestRecord records_[StorageCapacity];
  uint16_t count_;
  bool configured_;
  uint32_t nextSequence_;
  uint16_t nextGeneration_;
  bool identifiersExhausted_;

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
