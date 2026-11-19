#include "ca/cart/fixed_cart_claim_client.hpp"

namespace ca {

FixedCartClaimClient::FixedCartClaimClient(DeviceId cartId,
                                           TransactionId initialTransactionId)
    : cartId_(cartId), nextTransactionId_(initialTransactionId),
      exhausted_(initialTransactionId == 0), inFlight_(false),
      withdrawalRequested_(false), outstanding_() {}

Status FixedCartClaimClient::requestClaim(const RequestId& id, DeviceId cartId) {
  if (!RequestIdentity::isValid(id) || cartId == 0 || cartId != cartId_)
    return Status(ErrorCode::INVALID_ARGUMENT);
  if (inFlight_)
    return Status(ErrorCode::BUSY);
  if (exhausted_)
    return Status(ErrorCode::CAPACITY_EXCEEDED);

  outstanding_.requestId = id;
  outstanding_.cartId = cartId_;
  outstanding_.transactionId = nextTransactionId_;
  if (nextTransactionId_ == 0xFFFFFFFFUL)
    exhausted_ = true;
  else
    ++nextTransactionId_;

  inFlight_ = true;
  withdrawalRequested_ = false;
  return Status();
}

Status FixedCartClaimClient::withdrawClaim(const ClaimWithdraw& withdrawal) {
  if (!inFlight_)
    return Status(ErrorCode::NOT_FOUND);
  if (!RequestIdentity::equals(withdrawal.requestId, outstanding_.requestId) ||
      withdrawal.cartId != outstanding_.cartId ||
      withdrawal.transactionId != outstanding_.transactionId)
    return Status(ErrorCode::STALE_EVENT);
  if (withdrawalRequested_)
    return Status();
  withdrawalRequested_ = true;
  return Status();
}

Status FixedCartClaimClient::onClaimResult(const ClaimResult& result) {
  if (!inFlight_ ||
      !RequestIdentity::equals(result.requestId, outstanding_.requestId) ||
      result.transactionId != outstanding_.transactionId)
    return Status(ErrorCode::STALE_EVENT);
  if (static_cast<uint8_t>(result.finalState) >
      static_cast<uint8_t>(ClaimState::COMMITTED))
    return Status(ErrorCode::INVALID_ARGUMENT);

  if (result.result != ErrorCode::NONE ||
      result.finalState == ClaimState::WITHDRAWN ||
      result.finalState == ClaimState::COMMITTED) {
    clear();
    return Status();
  }
  if (result.finalState != ClaimState::PENDING)
    return Status(ErrorCode::INVALID_STATE);
  return Status();
}

Status FixedCartClaimClient::onAssignmentConfirmed(
    const AssignmentRecord& assignment) {
  if (!inFlight_ ||
      !RequestIdentity::equals(assignment.requestId, outstanding_.requestId) ||
      assignment.assignedCartId != cartId_ || assignment.epoch == 0 ||
      (assignment.state != RequestState::ASSIGNED &&
       assignment.state != RequestState::ON_THE_WAY &&
       assignment.state != RequestState::ARRIVED_AT_PICKUP &&
       assignment.state != RequestState::COMPLETED))
    return Status(ErrorCode::STALE_EVENT);
  clear();
  return Status();
}

bool FixedCartClaimClient::hasClaimInFlight() const { return inFlight_; }

bool FixedCartClaimClient::getOutstandingClaim(ClaimRequest& claim) const {
  if (!inFlight_)
    return false;
  claim = outstanding_;
  return true;
}

void FixedCartClaimClient::clear() {
  inFlight_ = false;
  withdrawalRequested_ = false;
  outstanding_ = ClaimRequest();
}

} // namespace ca
