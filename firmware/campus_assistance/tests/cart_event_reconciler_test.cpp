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

  const RequestId third = {1, 3, 1};
  RequestSnapshot snapshot = {};
  snapshot.requestId = third;
  snapshot.state = RequestState::ON_THE_WAY;
  snapshot.assignedCartId = 7;
  snapshot.assignmentEpoch = 2;
  snapshot.lastEventSequence = 1;
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
  return 0;
}
