#include "CampusAssistance.h"

#include <assert.h>
#include <string.h>

using namespace ca;

class FakeAtomicPersistence : public IPersistence {
public:
  FakeAtomicPersistence() : length_(0), present_(false), failNextStore_(false) {}

  Status
  load(RecordKey, uint8_t* output, uint16_t capacity, uint16_t& bytesRead) override {
    if (!present_)
      return Status(ErrorCode::NOT_FOUND);
    bytesRead = length_;
    if (capacity < length_)
      return Status(ErrorCode::BUFFER_FULL);
    memcpy(output, bytes_, length_);
    return Status();
  }

  Status storeAtomically(RecordKey, const uint8_t* data, uint16_t length) override {
    if (failNextStore_) {
      failNextStore_ = false;
      return Status(ErrorCode::STORAGE_FAILURE);
    }
    if (length > sizeof(bytes_))
      return Status(ErrorCode::BUFFER_FULL);
    // The fake models the adapter contract: a failed operation leaves the
    // previous committed value intact; success replaces it in one step.
    uint8_t staged[sizeof(bytes_)] = {};
    memcpy(staged, data, length);
    memcpy(bytes_, staged, length);
    length_ = length;
    present_ = true;
    return Status();
  }

  Status erase(RecordKey) override {
    present_ = false;
    length_ = 0;
    return Status();
  }

  void failNextStore() { failNextStore_ = true; }
  void corrupt(uint16_t index) {
    if (present_ && index < length_)
      bytes_[index] ^= 0x80U;
  }

private:
  uint8_t bytes_[512];
  uint16_t length_;
  bool present_;
  bool failNextStore_;
};

static RequestRecord makePending(const RequestId& id) {
  RequestRecord record = {};
  record.id = id;
  record.destinationId = 3;
  record.createdAtMs = 100;
  record.expiresAtMs = 600100;
  record.state = RequestState::PENDING_UNASSIGNED;
  record.claimState = ClaimState::IDLE;
  return record;
}

int main() {
  FakeAtomicPersistence persistence;
  RequestId allocated = {};

  {
    DurableRequestRepository<4> repository(persistence, 7, 1);
    assert(repository.configure().ok());
    assert(repository.reserveNextRequestId(1, allocated).ok());
    assert(allocated.originStationId == 1);
    assert(allocated.sequence == 1);
    assert(repository.insertNew(makePending(allocated)).ok());

    RequestId pendingId = {};
    assert(repository.reserveNextRequestId(1, pendingId).ok());
    assert(repository.insertNew(makePending(pendingId)).ok());

    RequestRecord updated = {};
    assert(repository.beginClaimIfEligible(allocated, 9, 55, 200, updated).ok());
    assert(repository.commitAssignmentIfClaimPending(allocated, 9, 55, 1, 201, updated)
               .ok());
  }

  // A newly constructed repository represents a reboot. It must recover both
  // the request state and the next identity without reusing the prior ID.
  DurableRequestRepository<4> recovered(persistence, 7, 1);
  assert(recovered.configure().ok());
  RequestRecord record = {};
  assert(recovered.find(allocated, record));
  assert(record.state == RequestState::ASSIGNED);
  assert(record.assignedCartId == 9);
  assert(record.assignmentEpoch == 1);
  RequestId pendingId = {};
  pendingId.originStationId = 1;
  pendingId.sequence = 2;
  pendingId.generation = 1;
  assert(recovered.find(pendingId, record));
  assert(record.state == RequestState::EXPIRED);

  RequestId next = {};
  assert(recovered.reserveNextRequestId(1, next).ok());
  assert(next.sequence == 3);

  JourneyUpdate update = {};
  update.requestId = allocated;
  update.assignedCartId = 9;
  update.status = JourneyStatus::ON_THE_WAY;
  update.eventSequence = 1;
  assert(recovered.applyJourneyUpdateIfCurrent(update, record).ok());

  // A simulated interrupted atomic write must not replace the last committed
  // snapshot. A second instance must recover the previous complete state.
  persistence.failNextStore();
  update.status = JourneyStatus::ARRIVED_AT_PICKUP;
  update.eventSequence = 2;
  assert(recovered.applyJourneyUpdateIfCurrent(update, record).code ==
         ErrorCode::STORAGE_FAILURE);

  DurableRequestRepository<4> afterInterruptedWrite(persistence, 7, 1);
  assert(afterInterruptedWrite.configure().ok());
  assert(afterInterruptedWrite.find(allocated, record));
  assert(record.state == RequestState::ON_THE_WAY);
  assert(record.lastJourneyEventSequence == 1);

  // The persisted station identity is part of the snapshot's authority.
  DurableRequestRepository<4> wrongStation(persistence, 7, 2);
  assert(wrongStation.configure().code == ErrorCode::STORAGE_FAILURE);

  // Corrupt snapshots fail closed; they must never silently initialize an
  // empty ledger and risk RequestId reuse.
  persistence.corrupt(5);
  DurableRequestRepository<4> corrupt(persistence, 7, 1);
  assert(corrupt.configure().code == ErrorCode::STORAGE_FAILURE);
  return 0;
}
