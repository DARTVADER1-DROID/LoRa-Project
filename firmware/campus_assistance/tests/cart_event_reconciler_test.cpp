#include "CampusAssistance.h"
#include "ca/cart/cart_event_reconciler.hpp"

#include <assert.h>

using namespace ca;

static RequestSummary summary(const RequestId& id) {
  RequestSummary request = {};
  request.id = id;
  request.destinationId = 3;
  return request;
}

int main() {
  FixedPendingRequestStore<4> pending;
  assert(pending.configure(4).ok());
  FixedActiveAssignment active;
  BasicDriverAvailabilityController availability;
  FixedCartClaimClient claims(7, 100);
  CartEventReconciler reconciler(7, pending, active, availability, claims);

  const RequestId first = {1, 1, 1};
  assert(pending.insertIfAbsent(summary(first)).ok());
  assert(claims.requestClaim(first, 7).ok());
  DriverAction enable = {DriverActionType::ENABLE_ACCEPTANCE, 10};
  assert(availability.enableAcceptance(enable).ok());
  assert(availability.onClaimStarted().ok());

  AssignmentNotice assignment = {};
  assignment.assignment.requestId = first;
  assignment.assignment.assignedCartId = 7;
  assignment.assignment.epoch = 1;
  assignment.assignment.state = RequestState::ASSIGNED;
  assert(reconciler.applyAssignmentNotice(assignment).ok());
  assert(!pending.contains(first));
  assert(active.isOccupied());
  assert(!claims.hasClaimInFlight());
  assert(availability.serviceState() == CartServiceState::SERVING);
  assert(availability.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);

  const RequestId second = {1, 2, 1};
  assert(pending.insertIfAbsent(summary(second)).ok());
  assert(claims.requestClaim(second, 7).ok());
  AssignmentNotice lost = {};
  lost.assignment.requestId = second;
  lost.assignment.assignedCartId = 8;
  lost.assignment.epoch = 1;
  lost.assignment.state = RequestState::ASSIGNED;
  assert(reconciler.applyAssignmentNotice(lost).ok());
  assert(!pending.contains(second));
  assert(!claims.hasClaimInFlight());

  // A cart that loses an in-flight claim returns from CLAIM_PENDING to IDLE.
  BasicDriverAvailabilityController losingAvailability;
  assert(losingAvailability.enableAcceptance(enable).ok());
  assert(losingAvailability.onClaimStarted().ok());
  FixedCartClaimClient losingClaims(9, 200);
  assert(losingClaims.requestClaim(second, 9).ok());
  FixedPendingRequestStore<2> losingPending;
  assert(losingPending.configure(2).ok());
  FixedActiveAssignment losingActive;
  CartEventReconciler losingReconciler(
      9, losingPending, losingActive, losingAvailability, losingClaims);
  assert(losingReconciler.applyAssignmentNotice(lost).ok());
  assert(!losingClaims.hasClaimInFlight());
  assert(losingAvailability.serviceState() == CartServiceState::IDLE);
  assert(losingAvailability.acceptanceMode() == AcceptanceMode::ACCEPTING);


  // A station snapshot preserves this cart's exact pending claim transaction.
  const RequestId pendingId = {1, 5, 1};
  FixedPendingRequestStore<2> claimPending;
  assert(claimPending.configure(2).ok());
  FixedActiveAssignment claimActive;
  BasicDriverAvailabilityController claimAvailability;
  FixedCartClaimClient ownClaim(7, 400);
  CartEventReconciler claimReconciler(
      7, claimPending, claimActive, claimAvailability, ownClaim);
  assert(ownClaim.requestClaim(pendingId, 7).ok());
  assert(claimAvailability.enableAcceptance(enable).ok());
  assert(claimAvailability.onClaimStarted().ok());

  RequestSnapshot claimPendingSnapshot = {};
  claimPendingSnapshot.requestId = pendingId;
  claimPendingSnapshot.state = RequestState::CLAIM_PENDING;
  claimPendingSnapshot.claimState = ClaimState::PENDING;
  claimPendingSnapshot.claimCartId = 7;
  claimPendingSnapshot.claimTransactionId = 400;
  assert(claimReconciler.applySnapshot(claimPendingSnapshot).ok());
  assert(ownClaim.hasClaimInFlight());
  assert(claimAvailability.serviceState() == CartServiceState::CLAIM_PENDING);

  // If the station says another cart owns the pending claim, resolve our claim.
  claimPendingSnapshot.claimCartId = 8;
  claimPendingSnapshot.claimTransactionId = 500;
  assert(claimReconciler.applySnapshot(claimPendingSnapshot).ok());
  assert(!ownClaim.hasClaimInFlight());
  assert(claimAvailability.serviceState() == CartServiceState::IDLE);

  const RequestId third = {1, 3, 1};
  RequestSnapshot snapshot = {};
  snapshot.requestId = third;
  snapshot.state = RequestState::ON_THE_WAY;
  snapshot.assignedCartId = 7;
  snapshot.assignmentEpoch = 2;
  snapshot.lastEventSequence = 1;
  snapshot.claimState = ClaimState::COMMITTED;
  snapshot.claimCartId = 7;
  snapshot.claimTransactionId = 301;
  assert(reconciler.applySnapshot(snapshot).ok());
  AssignmentRecord current = {};
  assert(active.get(current));
  assert(current.state == RequestState::ON_THE_WAY);

  snapshot.state = RequestState::ARRIVED_AT_PICKUP;
  snapshot.lastEventSequence = 2;
  assert(reconciler.applySnapshot(snapshot).ok());
  snapshot.state = RequestState::COMPLETED;
  snapshot.lastEventSequence = 3;
  assert(reconciler.applySnapshot(snapshot).ok());
  assert(!active.isOccupied());
  assert(availability.serviceState() == CartServiceState::IDLE);
  assert(availability.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);

  // A snapshot repairs missed intermediate journey events without replaying them.
  snapshot.requestId = {1, 4, 1};
  snapshot.state = RequestState::COMPLETED;
  snapshot.assignedCartId = 7;
  snapshot.assignmentEpoch = 3;
  snapshot.lastEventSequence = 3;
  snapshot.claimState = ClaimState::COMMITTED;
  snapshot.claimCartId = 7;
  snapshot.claimTransactionId = 302;
  assert(reconciler.applySnapshot(snapshot).ok());
  assert(!active.isOccupied());
  assert(availability.serviceState() == CartServiceState::IDLE);
  return 0;
}
