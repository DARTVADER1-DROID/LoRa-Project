#include "ca/cart/basic_driver_availability_controller.hpp"

namespace ca {

BasicDriverAvailabilityController::BasicDriverAvailabilityController()
    : acceptance_(AcceptanceMode::NOT_ACCEPTING), service_(CartServiceState::IDLE) {}

AcceptanceMode BasicDriverAvailabilityController::acceptanceMode() const {
  return acceptance_;
}

CartServiceState BasicDriverAvailabilityController::serviceState() const {
  return service_;
}

Status BasicDriverAvailabilityController::enableAcceptance(const DriverAction& action) {
  if (action.type != DriverActionType::ENABLE_ACCEPTANCE)
    return Status(ErrorCode::INVALID_ARGUMENT);
  if (service_ != CartServiceState::IDLE)
    return Status(ErrorCode::BUSY);
  acceptance_ = AcceptanceMode::ACCEPTING;
  return Status();
}

Status BasicDriverAvailabilityController::disableAcceptance(const DriverAction& action) {
  if (action.type != DriverActionType::DISABLE_ACCEPTANCE)
    return Status(ErrorCode::INVALID_ARGUMENT);

  acceptance_ = AcceptanceMode::NOT_ACCEPTING;
  if (service_ == CartServiceState::CLAIM_PENDING) {
    service_ = CartServiceState::UNCERTAIN;
    return Status(ErrorCode::COMMUNICATION_UNCERTAIN);
  }
  return Status();
}

Status BasicDriverAvailabilityController::onAssignmentConfirmed(
    const AssignmentRecord& assignment) {
  if (!RequestIdentity::isValid(assignment.requestId) ||
      assignment.assignedCartId == 0 || assignment.epoch == 0 ||
      assignment.state != RequestState::ASSIGNED)
    return Status(ErrorCode::INVALID_ARGUMENT);
  if (service_ == CartServiceState::SERVING)
    return Status(ErrorCode::BUSY);

  acceptance_ = AcceptanceMode::NOT_ACCEPTING;
  service_ = CartServiceState::SERVING;
  return Status();
}

Status BasicDriverAvailabilityController::onJourneyCompleted() {
  if (service_ != CartServiceState::SERVING)
    return Status(ErrorCode::INVALID_STATE);
  service_ = CartServiceState::IDLE;
  acceptance_ = AcceptanceMode::NOT_ACCEPTING;
  return Status();
}

bool BasicDriverAvailabilityController::mayClaim() const {
  return acceptance_ == AcceptanceMode::ACCEPTING &&
         service_ == CartServiceState::IDLE;
}

Status BasicDriverAvailabilityController::onClaimStarted() {
  if (!mayClaim())
    return Status(ErrorCode::NOT_ACCEPTING);
  service_ = CartServiceState::CLAIM_PENDING;
  return Status();
}

Status BasicDriverAvailabilityController::onClaimWithdrawnByStation() {
  if (service_ != CartServiceState::UNCERTAIN &&
      service_ != CartServiceState::CLAIM_PENDING)
    return Status(ErrorCode::INVALID_STATE);
  service_ = CartServiceState::IDLE;
  return Status();
}

} // namespace ca
