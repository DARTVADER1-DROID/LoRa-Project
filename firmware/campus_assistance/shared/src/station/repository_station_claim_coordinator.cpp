#include "ca/station/repository_station_claim_coordinator.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

RepositoryStationClaimCoordinator::RepositoryStationClaimCoordinator(
    IRequestRepository& repository)
    : repository_(repository) {}

Status RepositoryStationClaimCoordinator::evaluate(const ClaimRequest& claim,
                                                    uint32_t stationNowMs) {
  if (!RequestIdentity::isValid(claim.requestId) || claim.cartId == 0 ||
      claim.transactionId == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);

  RequestRecord updated = {};
  return repository_.beginClaimIfEligible(claim.requestId, claim.cartId,
                                          claim.transactionId, stationNowMs,
                                          updated);
}

Status RepositoryStationClaimCoordinator::withdrawClaim(
    const ClaimWithdraw& withdrawal) {
  if (!RequestIdentity::isValid(withdrawal.requestId) || withdrawal.cartId == 0 ||
      withdrawal.transactionId == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);

  RequestRecord updated = {};
  return repository_.withdrawClaimIfPending(
      withdrawal.requestId, withdrawal.cartId, withdrawal.transactionId, updated);
}

Status RepositoryStationClaimCoordinator::commitWinner(
    const ClaimRequest& claim, uint32_t stationNowMs,
    AssignmentRecord& assignment) {
  if (!RequestIdentity::isValid(claim.requestId) || claim.cartId == 0 ||
      claim.transactionId == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);

  // Assignment is one-shot per RequestId in the current lifecycle. Epoch 1 is
  // therefore stable across retries and reboot recovery; reassignment would
  // require a separately specified durable epoch allocator and policy.
  RequestRecord updated = {};
  const Status committed = repository_.commitAssignmentIfClaimPending(
      claim.requestId, claim.cartId, claim.transactionId, 1, stationNowMs, updated);
  if (!committed.ok())
    return committed;

  assignment.requestId = updated.id;
  assignment.assignedCartId = updated.assignedCartId;
  assignment.epoch = updated.assignmentEpoch;
  assignment.state = updated.state;
  return Status();
}

bool RepositoryStationClaimCoordinator::isAssigned(const RequestId& id) const {
  RequestRecord record = {};
  if (!repository_.find(id, record))
    return false;
  return record.state == RequestState::ASSIGNED ||
         record.state == RequestState::ON_THE_WAY ||
         record.state == RequestState::ARRIVED_AT_PICKUP ||
         record.state == RequestState::COMPLETED;
}

} // namespace ca
