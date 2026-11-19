#include "CampusAssistance.h"

#include <assert.h>
#include <string.h>

using namespace ca;

namespace {

bool sameId(const RequestId& a, const RequestId& b) {
  return a.originStationId == b.originStationId && a.sequence == b.sequence &&
         a.generation == b.generation;
}

class FakeRequestRepository : public IRequestRepository {
public:
  FakeRequestRepository() : count_(0) { memset(records_, 0, sizeof(records_)); }

  Status insertNew(const RequestRecord& record) override {
    if (count_ >= kCapacity)
      return Status(ErrorCode::CAPACITY_EXCEEDED);
    RequestRecord ignored;
    if (find(record.id, ignored))
      return Status(ErrorCode::DUPLICATE);
    records_[count_++] = record;
    return Status();
  }

  bool find(const RequestId& id, RequestRecord& record) const override {
    for (uint8_t i = 0; i < count_; ++i) {
      if (sameId(records_[i].id, id)) {
        record = records_[i];
        return true;
      }
    }
    return false;
  }

  Status beginClaimIfEligible(const RequestId& id,
                              DeviceId cartId,
                              TransactionId transactionId,
                              uint32_t now,
                              RequestRecord& updated) override {
    RequestRecord* r = mutableFind(id);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (isDue(*r, now)) {
      r->state = RequestState::EXPIRED;
      r->claimState = ClaimState::WITHDRAWN;
      updated = *r;
      return Status(ErrorCode::EXPIRED);
    }
    if (r->state != RequestState::PENDING_UNASSIGNED ||
        (r->claimState != ClaimState::IDLE && r->claimState != ClaimState::WITHDRAWN))
      return Status(ErrorCode::INVALID_STATE);
    if (transactionId == 0 || cartId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    r->claimState = ClaimState::PENDING;
    r->claimCartId = cartId;
    r->claimTransactionId = transactionId;
    r->state = RequestState::CLAIM_PENDING;
    updated = *r;
    return Status();
  }

  Status withdrawClaimIfPending(const RequestId& id,
                                DeviceId cartId,
                                TransactionId transactionId,
                                RequestRecord& updated) override {
    RequestRecord* r = mutableFind(id);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (r->claimState == ClaimState::COMMITTED)
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (r->claimState == ClaimState::WITHDRAWN && r->claimCartId == cartId &&
        r->claimTransactionId == transactionId) {
      updated = *r;
      return Status();
    }
    if (r->claimState != ClaimState::PENDING || r->claimCartId != cartId ||
        r->claimTransactionId != transactionId)
      return Status(ErrorCode::STALE_EVENT);
    r->claimState = ClaimState::WITHDRAWN;
    r->state = RequestState::PENDING_UNASSIGNED;
    updated = *r;
    return Status();
  }

  Status commitAssignmentIfClaimPending(const RequestId& id,
                                        DeviceId cartId,
                                        TransactionId transactionId,
                                        AssignmentEpoch epoch,
                                        uint32_t now,
                                        RequestRecord& updated) override {
    RequestRecord* r = mutableFind(id);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (isDue(*r, now)) {
      r->state = RequestState::EXPIRED;
      r->claimState = ClaimState::WITHDRAWN;
      updated = *r;
      return Status(ErrorCode::EXPIRED);
    }
    if (r->claimState == ClaimState::COMMITTED && r->claimCartId == cartId &&
        r->claimTransactionId == transactionId) {
      updated = *r;
      return Status();
    }
    if (r->claimState != ClaimState::PENDING || r->claimCartId != cartId ||
        r->claimTransactionId != transactionId ||
        r->state != RequestState::CLAIM_PENDING)
      return Status(ErrorCode::STALE_EVENT);
    r->claimState = ClaimState::COMMITTED;
    r->state = RequestState::ASSIGNED;
    r->assignedCartId = cartId;
    r->assignmentEpoch = epoch;
    updated = *r;
    return Status();
  }

  Status cancelIfUnassigned(const RequestId& id,
                            CancellationSource,
                            RequestRecord& updated) override {
    RequestRecord* r = mutableFind(id);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (r->state == RequestState::ASSIGNED || r->state == RequestState::ON_THE_WAY ||
        r->state == RequestState::ARRIVED_AT_PICKUP ||
        r->state == RequestState::COMPLETED)
      return Status(ErrorCode::ALREADY_ASSIGNED);
    if (r->state == RequestState::CANCELLED) {
      updated = *r;
      return Status();
    }
    if (r->state == RequestState::EXPIRED)
      return Status(ErrorCode::EXPIRED);
    r->state = RequestState::CANCELLED;
    if (r->claimState == ClaimState::PENDING)
      r->claimState = ClaimState::WITHDRAWN;
    updated = *r;
    return Status();
  }

  Status
  expireIfDue(const RequestId& id, uint32_t now, RequestRecord& updated) override {
    RequestRecord* r = mutableFind(id);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (!isDue(*r, now))
      return Status(ErrorCode::INVALID_STATE);
    if (r->state == RequestState::ASSIGNED || r->state == RequestState::ON_THE_WAY ||
        r->state == RequestState::ARRIVED_AT_PICKUP ||
        r->state == RequestState::COMPLETED)
      return Status(ErrorCode::ALREADY_ASSIGNED);
    r->state = RequestState::EXPIRED;
    if (r->claimState == ClaimState::PENDING)
      r->claimState = ClaimState::WITHDRAWN;
    updated = *r;
    return Status();
  }

  Status applyJourneyUpdateIfCurrent(const JourneyUpdate& update,
                                     RequestRecord& updated) override {
    RequestRecord* r = mutableFind(update.requestId);
    if (!r)
      return Status(ErrorCode::NOT_FOUND);
    if (r->assignedCartId != update.assignedCartId ||
        r->state == RequestState::CANCELLED || r->state == RequestState::EXPIRED)
      return Status(ErrorCode::STALE_EVENT);
    if (update.eventSequence <= r->lastJourneyEventSequence)
      return Status(ErrorCode::STALE_EVENT);
    r->lastJourneyEventSequence = update.eventSequence;
    r->state = update.status == JourneyStatus::ON_THE_WAY
                   ? RequestState::ON_THE_WAY
                   : (update.status == JourneyStatus::ARRIVED_AT_PICKUP
                          ? RequestState::ARRIVED_AT_PICKUP
                          : (update.status == JourneyStatus::COMPLETED
                                 ? RequestState::COMPLETED
                                 : RequestState::ASSIGNED));
    updated = *r;
    return Status();
  }

  Status removeTerminalBefore(uint32_t) override { return Status(); }
  uint16_t size() const override { return count_; }

private:
  static const uint8_t kCapacity = 4;
  RequestRecord records_[kCapacity];
  uint8_t count_;

  static bool isDue(const RequestRecord& r, uint32_t now) {
    return static_cast<int32_t>(now - r.expiresAtMs) >= 0;
  }

  RequestRecord* mutableFind(const RequestId& id) {
    for (uint8_t i = 0; i < count_; ++i)
      if (sameId(records_[i].id, id))
        return &records_[i];
    return 0;
  }
};

RequestRecord makeRequest(uint32_t expiry) {
  RequestRecord r = {};
  r.id.originStationId = 10;
  r.id.sequence = 1;
  r.id.generation = 1;
  r.destinationId = 20;
  r.createdAtMs = 100;
  r.expiresAtMs = expiry;
  r.state = RequestState::PENDING_UNASSIGNED;
  r.claimState = ClaimState::IDLE;
  return r;
}

void testWithdrawWinsAgainstCommit() {
  FakeRequestRepository repository;
  RequestRecord request = makeRequest(1000), updated = {};
  assert(repository.insertNew(request).ok());
  assert(repository.beginClaimIfEligible(request.id, 7, 101, 200, updated).ok());
  assert(repository.withdrawClaimIfPending(request.id, 7, 101, updated).ok());
  assert(repository.commitAssignmentIfClaimPending(request.id, 7, 101, 1, 201, updated)
             .code == ErrorCode::STALE_EVENT);
  assert(repository.find(request.id, updated));
  assert(updated.state == RequestState::PENDING_UNASSIGNED);
  assert(updated.claimState == ClaimState::WITHDRAWN);
}

void testCommitWinsAgainstDelayedWithdraw() {
  FakeRequestRepository repository;
  RequestRecord request = makeRequest(1000), updated = {};
  assert(repository.insertNew(request).ok());
  assert(repository.beginClaimIfEligible(request.id, 8, 202, 200, updated).ok());
  assert(repository.commitAssignmentIfClaimPending(request.id, 8, 202, 2, 201, updated)
             .ok());
  assert(repository.withdrawClaimIfPending(request.id, 8, 202, updated).code ==
         ErrorCode::ALREADY_ASSIGNED);
  assert(repository.find(request.id, updated));
  assert(updated.assignedCartId == 8);
  assert(updated.claimState == ClaimState::COMMITTED);
}

void testCancellationAndExpiryClosePendingClaim() {
  FakeRequestRepository cancelledRepo, expiredRepo;
  RequestRecord request = makeRequest(1000), updated = {};
  assert(cancelledRepo.insertNew(request).ok());
  assert(cancelledRepo.beginClaimIfEligible(request.id, 3, 303, 200, updated).ok());
  assert(
      cancelledRepo.cancelIfUnassigned(request.id, CancellationSource::STUDENT, updated)
          .ok());
  assert(
      cancelledRepo.commitAssignmentIfClaimPending(request.id, 3, 303, 1, 201, updated)
          .code == ErrorCode::STALE_EVENT);

  assert(expiredRepo.insertNew(request).ok());
  assert(expiredRepo.beginClaimIfEligible(request.id, 4, 404, 200, updated).ok());
  assert(expiredRepo.expireIfDue(request.id, 1000, updated).ok());
  assert(
      expiredRepo.commitAssignmentIfClaimPending(request.id, 4, 404, 1, 1001, updated)
          .code == ErrorCode::EXPIRED);
}

void testExpiredRequestCannotBeClaimed() {
  FakeRequestRepository repository;
  RequestRecord request = makeRequest(500), updated = {};
  assert(repository.insertNew(request).ok());
  assert(repository.beginClaimIfEligible(request.id, 9, 909, 500, updated).code ==
         ErrorCode::EXPIRED);
  assert(repository.find(request.id, updated));
  assert(updated.state == RequestState::EXPIRED);
}

} // namespace

int main() {
  testWithdrawWinsAgainstCommit();
  testCommitWinsAgainstDelayedWithdraw();
  testCancellationAndExpiryClosePendingClaim();
  testExpiredRequestCannotBeClaimed();
  return 0;
}
