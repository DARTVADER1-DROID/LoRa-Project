#include "CampusAssistance.h"
#include <assert.h>
using namespace ca;
static RequestRecord makeRecord(uint32_t lifetime = 600000UL) {
  RequestRecord r = {};
  r.id.originStationId = 1;
  r.id.sequence = 1;
  r.id.generation = 1;
  r.destinationId = 2;
  r.createdAtMs = 100;
  r.expiresAtMs = 100 + lifetime;
  r.state = RequestState::PENDING_UNASSIGNED;
  r.claimState = ClaimState::IDLE;
  return r;
}
int main() {
  FixedRequestRepository<3> repo;
  RequestRecord r = makeRecord(), out = {};
  assert(repo.insertNew(r).code == ErrorCode::INVALID_STATE);
  assert(repo.configure().ok());
  assert(repo.insertNew(r).ok());
  assert(repo.configure().code == ErrorCode::INVALID_STATE);
  assert(repo.find(r.id, out));
  assert(repo.insertNew(r).code == ErrorCode::DUPLICATE);
  assert(repo.beginClaimIfEligible(r.id, 8, 80, 200, out).ok());
  assert(repo.withdrawClaimIfPending(r.id, 8, 80, out).ok());
  assert(repo.commitAssignmentIfClaimPending(r.id, 8, 80, 1, 201, out).code ==
         ErrorCode::STALE_EVENT);
  assert(repo.beginClaimIfEligible(r.id, 9, 90, 202, out).ok());
  assert(repo.commitAssignmentIfClaimPending(r.id, 9, 90, 1, 203, out).ok());
  assert(repo.withdrawClaimIfPending(r.id, 9, 90, out).code ==
         ErrorCode::ALREADY_ASSIGNED);
  assert(repo.commitAssignmentIfClaimPending(r.id, 9, 90, 1, 999999, out).ok());
  assert(repo.commitAssignmentIfClaimPending(r.id, 9, 90, 2, 999999, out).code ==
         ErrorCode::STALE_EVENT);
  assert(out.assignedCartId == 9);
  JourneyUpdate j = {};
  j.requestId = r.id;
  j.assignedCartId = 9;
  j.eventSequence = 1;
  j.status = JourneyStatus::ARRIVED_AT_PICKUP;
  assert(repo.applyJourneyUpdateIfCurrent(j, out).code == ErrorCode::INVALID_STATE);
  j.status = JourneyStatus::ON_THE_WAY;
  assert(repo.applyJourneyUpdateIfCurrent(j, out).ok());
  j.status = JourneyStatus::ARRIVED_AT_PICKUP;
  j.eventSequence = 2;
  assert(repo.applyJourneyUpdateIfCurrent(j, out).ok());
  j.status = JourneyStatus::COMPLETED;
  j.eventSequence = 3;
  assert(repo.applyJourneyUpdateIfCurrent(j, out).ok());
  assert(out.state == RequestState::COMPLETED);

  // A withdrawal can arrive before its claim after RF retransmission/reordering.
  FixedRequestRepository<1> withdrawalTombstone;
  RequestRecord tombstoneRecord = makeRecord();
  tombstoneRecord.id.sequence = 2;
  assert(withdrawalTombstone.configure().ok());
  assert(withdrawalTombstone.insertNew(tombstoneRecord).ok());
  assert(withdrawalTombstone.withdrawClaimIfPending(tombstoneRecord.id, 12, 120, out)
             .ok());
  assert(withdrawalTombstone.withdrawClaimIfPending(tombstoneRecord.id, 12, 120, out)
             .ok());
  assert(withdrawalTombstone.beginClaimIfEligible(tombstoneRecord.id, 12, 120, 101, out)
             .code == ErrorCode::STALE_EVENT);
  assert(withdrawalTombstone.beginClaimIfEligible(tombstoneRecord.id, 13, 130, 102, out)
             .ok());
  assert(out.claimCartId == 13);
  assert(out.claimTransactionId == 130);

  FixedRequestRepository<1> wrap;
  RequestRecord w = makeRecord();
  w.createdAtMs = 0xFFFFFF00UL;
  w.expiresAtMs = w.createdAtMs + 600000UL;
  assert(wrap.configure().ok());
  assert(wrap.insertNew(w).ok());
  assert(wrap.expireIfDue(w.id, w.expiresAtMs - 1, out).code ==
         ErrorCode::INVALID_STATE);
  assert(wrap.expireIfDue(w.id, w.expiresAtMs, out).ok());
  FixedRequestRepository<1> cancel;
  RequestRecord c = makeRecord();
  assert(cancel.configure().ok());
  assert(cancel.insertNew(c).ok());
  assert(cancel.cancelIfUnassigned(c.id, static_cast<CancellationSource>(0xFF), out)
             .code == ErrorCode::INVALID_ARGUMENT);
  assert(cancel.cancelIfUnassigned(c.id, CancellationSource::STUDENT, out).ok());
  assert(cancel.beginClaimIfEligible(c.id, 3, 3, 101, out).code ==
         ErrorCode::CANCELLED);

  FixedRequestRepository<3> retention;
  RequestRecord oldTerminal = makeRecord();
  RequestRecord recentTerminal = makeRecord();
  RequestRecord live = makeRecord();
  recentTerminal.id.sequence = 2;
  recentTerminal.createdAtMs = 300;
  recentTerminal.expiresAtMs = 900;
  live.id.sequence = 3;
  live.createdAtMs = 400;
  live.expiresAtMs = 1000;
  assert(retention.configure().ok());
  assert(retention.insertNew(oldTerminal).ok());
  assert(retention.insertNew(recentTerminal).ok());
  assert(retention.insertNew(live).ok());
  assert(retention.cancelIfUnassigned(oldTerminal.id, CancellationSource::STUDENT, out)
             .ok());
  assert(
      retention.cancelIfUnassigned(recentTerminal.id, CancellationSource::STUDENT, out)
          .ok());
  assert(retention.removeTerminalBefore(200).ok());
  assert(retention.size() == 2);
  assert(!retention.find(oldTerminal.id, out));
  assert(retention.find(recentTerminal.id, out));
  assert(retention.find(live.id, out));

  FixedRequestRepository<2> capacityCompaction;
  RequestRecord terminalForEviction = makeRecord();
  RequestRecord liveForEviction = makeRecord();
  RequestRecord incoming = makeRecord();
  RequestRecord overflow = makeRecord();
  liveForEviction.id.sequence = 2;
  incoming.id.sequence = 3;
  overflow.id.sequence = 4;
  assert(capacityCompaction.configure().ok());
  assert(capacityCompaction.insertNew(terminalForEviction).ok());
  assert(capacityCompaction.insertNew(liveForEviction).ok());
  assert(capacityCompaction.cancelIfUnassigned(
             terminalForEviction.id, CancellationSource::STUDENT, out)
             .ok());
  assert(capacityCompaction.insertNew(incoming).ok());
  assert(capacityCompaction.size() == 2);
  assert(!capacityCompaction.find(terminalForEviction.id, out));
  assert(capacityCompaction.find(liveForEviction.id, out));
  assert(capacityCompaction.find(incoming.id, out));
  assert(capacityCompaction.insertNew(overflow).code ==
         ErrorCode::CAPACITY_EXCEEDED);
  return 0;
}
