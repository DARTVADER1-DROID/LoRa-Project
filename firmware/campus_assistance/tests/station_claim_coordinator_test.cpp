#include "CampusAssistance.h"
#include "ca/station/repository_station_claim_coordinator.hpp"

#include <assert.h>

using namespace ca;

static RequestRecord makeRequest() {
  RequestRecord record = {};
  record.id = {1, 1, 1};
  record.destinationId = 4;
  record.createdAtMs = 100;
  record.expiresAtMs = 1000;
  record.state = RequestState::PENDING_UNASSIGNED;
  record.claimState = ClaimState::IDLE;
  return record;
}

int main() {
  FixedRequestRepository<2> repository;
  assert(repository.configure().ok());
  RequestRecord request = makeRequest();
  assert(repository.insertNew(request).ok());

  RepositoryStationClaimCoordinator coordinator(repository);
  ClaimRequest first = {};
  first.requestId = request.id;
  first.cartId = 7;
  first.transactionId = 70;
  assert(coordinator.evaluate(first, 200).ok());

  // Retransmitted CLAIM with the same transaction is idempotent.
  assert(coordinator.evaluate(first, 201).ok());

  ClaimRequest competing = first;
  competing.cartId = 8;
  competing.transactionId = 80;
  assert(coordinator.evaluate(competing, 201).code == ErrorCode::BUSY);

  ClaimWithdraw withdrawal = {};
  withdrawal.requestId = first.requestId;
  withdrawal.cartId = first.cartId;
  withdrawal.transactionId = first.transactionId;
  assert(coordinator.withdrawClaim(withdrawal).ok());
  assert(coordinator.withdrawClaim(withdrawal).ok());
  AssignmentRecord rejectedAssignment = {};
  assert(coordinator.commitWinner(first, 202, rejectedAssignment).code ==
         ErrorCode::STALE_EVENT);

  assert(coordinator.evaluate(competing, 203).ok());
  AssignmentRecord assignment = {};
  assert(coordinator.commitWinner(competing, 204, assignment).ok());
  assert(assignment.assignedCartId == 8);
  assert(assignment.epoch == 1);
  assert(assignment.state == RequestState::ASSIGNED);
  assert(coordinator.isAssigned(request.id));
  AssignmentRecord recovered = {};
  assert(coordinator.getAssignment(request.id, recovered));
  assert(recovered.assignedCartId == 8);
  assert(recovered.epoch == 1);

  // A delayed withdrawal cannot undo a committed winner.
  withdrawal.cartId = competing.cartId;
  withdrawal.transactionId = competing.transactionId;
  assert(coordinator.withdrawClaim(withdrawal).code == ErrorCode::ALREADY_ASSIGNED);
  assert(coordinator.commitWinner(competing, 9000, assignment).ok());
  assert(assignment.assignedCartId == 8);
  return 0;
}
