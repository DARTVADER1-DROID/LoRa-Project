#include "ca/cart/cart_event_reconciler.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

CartEventReconciler::CartEventReconciler(DeviceId cartId,
                                         PendingRequestStore& pendingRequests,
                                         ActiveAssignment& activeAssignment,
                                         DriverAvailabilityController& availability,
                                         CartClaimClient& claimClient)
    : cartId_(cartId), pendingRequests_(pendingRequests),
      activeAssignment_(activeAssignment), availability_(availability),
      claimClient_(claimClient) {
}

Status CartEventReconciler::applyAssignmentNotice(const AssignmentNotice& notice) {
  const AssignmentRecord& assignment = notice.assignment;
  if (!RequestIdentity::isValid(assignment.requestId) ||
      assignment.assignedCartId == 0 || assignment.epoch == 0 ||
      assignment.state != RequestState::ASSIGNED)
    return Status(ErrorCode::INVALID_ARGUMENT);

  pendingRequests_.removeAssigned(assignment.requestId);
  if (assignment.assignedCartId != cartId_) {
    resolveLostClaim(assignment.requestId, ErrorCode::ALREADY_ASSIGNED);
    return Status();
  }

  const Status activated = activeAssignment_.activate(assignment);
  if (!activated.ok())
    return activated;
  const Status availability = availability_.onAssignmentConfirmed(assignment);
  if (!availability.ok())
    return availability;
  if (claimClient_.hasClaimInFlight())
    return claimClient_.onAssignmentConfirmed(assignment);
  return Status();
}

Status CartEventReconciler::applyCancellationNotice(const CancellationNotice& notice) {
  if (!RequestIdentity::isValid(notice.requestId) ||
      static_cast<uint8_t>(notice.source) >
          static_cast<uint8_t>(CancellationSource::SYSTEM))
    return Status(ErrorCode::INVALID_ARGUMENT);
  pendingRequests_.remove(notice.requestId);
  resolveLostClaim(notice.requestId, ErrorCode::CANCELLED);
  return Status();
}

Status CartEventReconciler::applyExpiryNotice(const ExpiryNotice& notice) {
  if (!RequestIdentity::isValid(notice.requestId))
    return Status(ErrorCode::INVALID_ARGUMENT);
  pendingRequests_.remove(notice.requestId);
  resolveLostClaim(notice.requestId, ErrorCode::EXPIRED);
  return Status();
}

Status CartEventReconciler::applySnapshot(const RequestSnapshot& snapshot) {
  if (!RequestIdentity::isValid(snapshot.requestId) ||
      static_cast<uint8_t>(snapshot.state) >
          static_cast<uint8_t>(RequestState::COMMUNICATION_UNCERTAIN))
    return Status(ErrorCode::INVALID_ARGUMENT);

  const bool assigned = snapshot.state == RequestState::ASSIGNED ||
                        snapshot.state == RequestState::ON_THE_WAY ||
                        snapshot.state == RequestState::ARRIVED_AT_PICKUP ||
                        snapshot.state == RequestState::COMPLETED;
  if (!assigned) {
    if (snapshot.assignedCartId != 0 || snapshot.assignmentEpoch != 0)
      return Status(ErrorCode::INVALID_ARGUMENT);

    if (snapshot.state == RequestState::CLAIM_PENDING) {
      if (snapshot.claimState != ClaimState::PENDING ||
          snapshot.claimCartId == 0 || snapshot.claimTransactionId == 0)
        return Status(ErrorCode::INVALID_ARGUMENT);
      ClaimRequest outstanding = {};
      if (claimClient_.getOutstandingClaim(outstanding) &&
          RequestIdentity::equals(outstanding.requestId, snapshot.requestId)) {
        if (outstanding.cartId == snapshot.claimCartId &&
            outstanding.transactionId == snapshot.claimTransactionId)
          return Status();
        resolveLostClaim(snapshot.requestId, ErrorCode::STALE_EVENT);
      }
      return Status();
    }

    if (snapshot.claimState != ClaimState::IDLE &&
        snapshot.claimState != ClaimState::WITHDRAWN)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if ((snapshot.claimState == ClaimState::IDLE &&
         (snapshot.claimCartId != 0 || snapshot.claimTransactionId != 0)) ||
        (snapshot.claimState == ClaimState::WITHDRAWN &&
         (snapshot.claimCartId == 0 || snapshot.claimTransactionId == 0)))
      return Status(ErrorCode::INVALID_ARGUMENT);

    if (snapshot.state == RequestState::CANCELLED ||
        snapshot.state == RequestState::EXPIRED) {
      pendingRequests_.remove(snapshot.requestId);
      resolveLostClaim(snapshot.requestId,
                       snapshot.state == RequestState::CANCELLED ? ErrorCode::CANCELLED
                                                                 : ErrorCode::EXPIRED);
    } else if (snapshot.state == RequestState::PENDING_UNASSIGNED) {
      resolveLostClaim(snapshot.requestId, ErrorCode::STALE_EVENT);
    } else {
      return Status(ErrorCode::INVALID_ARGUMENT);
    }
    return Status();
  }

  if (snapshot.assignedCartId == 0 || snapshot.assignmentEpoch == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);
  pendingRequests_.removeAssigned(snapshot.requestId);
  if (snapshot.assignedCartId != cartId_) {
    resolveLostClaim(snapshot.requestId, ErrorCode::ALREADY_ASSIGNED);
    return Status();
  }

  AssignmentRecord assignment = {};
  assignment.requestId = snapshot.requestId;
  assignment.assignedCartId = cartId_;
  assignment.epoch = snapshot.assignmentEpoch;
  assignment.state = RequestState::ASSIGNED;
  const Status activated = activeAssignment_.activate(assignment);
  if (!activated.ok())
    return activated;
  const Status availability = availability_.onAssignmentConfirmed(assignment);
  if (!availability.ok())
    return availability;
  if (claimClient_.hasClaimInFlight())
    claimClient_.onAssignmentConfirmed(assignment);

  if (snapshot.state != RequestState::ASSIGNED) {
    const JourneyStatus journey =
        snapshot.state == RequestState::ON_THE_WAY
            ? JourneyStatus::ON_THE_WAY
            : (snapshot.state == RequestState::ARRIVED_AT_PICKUP
                   ? JourneyStatus::ARRIVED_AT_PICKUP
                   : JourneyStatus::COMPLETED);
    const Status update =
        activeAssignment_.reconcileSnapshot(journey, snapshot.lastEventSequence);
    if (!update.ok() && update.code != ErrorCode::STALE_EVENT)
      return update;

    if (snapshot.state == RequestState::COMPLETED) {
      if (availability_.serviceState() == CartServiceState::SERVING)
        availability_.onJourneyCompleted();
      if (activeAssignment_.isOccupied())
        activeAssignment_.clearAfterTerminal();
    }
  }
  return Status();
}

void CartEventReconciler::resolveLostClaim(const RequestId& id, ErrorCode reason) {
  if (!claimClient_.hasClaimInFlight())
    return;
  ClaimRequest claim = {};
  if (!claimClient_.getOutstandingClaim(claim) ||
      !RequestIdentity::equals(claim.requestId, id))
    return;

  ClaimResult result = {};
  result.requestId = id;
  result.transactionId = claim.transactionId;
  result.result = reason;
  result.finalState = reason == ErrorCode::ALREADY_ASSIGNED ? ClaimState::COMMITTED
                                                            : ClaimState::WITHDRAWN;
  const Status resolved = claimClient_.onClaimResult(result);
  if (resolved.ok())
    availability_.onClaimWithdrawnByStation();
}

} // namespace ca
