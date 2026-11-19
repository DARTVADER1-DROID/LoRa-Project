#pragma once

#include "domain.hpp"

namespace ca {

// Explicit driver intent. This class owns acceptance/service dimensions only;
// the active assignment itself remains owned by ActiveAssignment.
class BasicDriverAvailabilityController final : public DriverAvailabilityController {
public:
  BasicDriverAvailabilityController()
      : acceptance_(AcceptanceMode::NOT_ACCEPTING),
        service_(CartServiceState::IDLE), claimOutstanding_(false) {}

  AcceptanceMode acceptanceMode() const override { return acceptance_; }
  CartServiceState serviceState() const override { return service_; }

  Status enableAcceptance(const DriverAction& action) override {
    if (action.type != DriverActionType::ENABLE_ACCEPTANCE)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (service_ != CartServiceState::IDLE || claimOutstanding_)
      return Status(ErrorCode::BUSY);
    acceptance_ = AcceptanceMode::ACCEPTING;
    return Status();
  }

  Status disableAcceptance(const DriverAction& action) override {
    if (action.type != DriverActionType::DISABLE_ACCEPTANCE)
      return Status(ErrorCode::INVALID_ARGUMENT);
    acceptance_ = AcceptanceMode::NOT_ACCEPTING;
    if (claimOutstanding_) {
      service_ = CartServiceState::UNCERTAIN;
      return Status(ErrorCode::COMMUNICATION_UNCERTAIN);
    }
    if (service_ == CartServiceState::CLAIM_PENDING)
      service_ = CartServiceState::IDLE;
    return Status();
  }

  Status onAssignmentConfirmed(const AssignmentRecord& assignment) override {
    if (!RequestIdentity::isValid(assignment.requestId) ||
        assignment.assignedCartId == 0 || assignment.epoch == 0 ||
        assignment.state != RequestState::ASSIGNED)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (service_ == CartServiceState::SERVING)
      return Status(ErrorCode::BUSY);
    acceptance_ = AcceptanceMode::NOT_ACCEPTING;
    service_ = CartServiceState::SERVING;
    claimOutstanding_ = false;
    return Status();
  }

  Status onJourneyCompleted() override {
    if (service_ != CartServiceState::SERVING)
      return Status(ErrorCode::INVALID_STATE);
    service_ = CartServiceState::IDLE;
    acceptance_ = AcceptanceMode::NOT_ACCEPTING;
    claimOutstanding_ = false;
    return Status();
  }

  bool mayClaim() const override {
    return acceptance_ == AcceptanceMode::ACCEPTING &&
           service_ == CartServiceState::IDLE && !claimOutstanding_;
  }

  // Called by the claim transaction owner; no public UI action can forge it.
  Status setClaimOutstanding(bool outstanding) {
    if (outstanding) {
      if (!mayClaim())
        return Status(ErrorCode::NOT_ACCEPTING);
      claimOutstanding_ = true;
      service_ = CartServiceState::CLAIM_PENDING;
      return Status();
    }
    if (service_ == CartServiceState::UNCERTAIN)
      return Status(ErrorCode::COMMUNICATION_UNCERTAIN);
    claimOutstanding_ = false;
    if (service_ == CartServiceState::CLAIM_PENDING)
      service_ = CartServiceState::IDLE;
    return Status();
  }

  Status onClaimWithdrawnByStation() {
    if (!claimOutstanding_ || service_ != CartServiceState::UNCERTAIN)
      return Status(ErrorCode::INVALID_STATE);
    claimOutstanding_ = false;
    service_ = CartServiceState::IDLE;
    return Status();
  }

private:
  AcceptanceMode acceptance_;
  CartServiceState service_;
  bool claimOutstanding_;
};

class FixedActiveAssignment final : public ActiveAssignment {
public:
  FixedActiveAssignment()
      : occupied_(false), journey_(JourneyStatus::RECEIVED),
        lastSequence_(0) {}

  bool isOccupied() const override { return occupied_; }

  Status activate(const AssignmentRecord& assignment) override {
    if (!RequestIdentity::isValid(assignment.requestId) ||
        assignment.assignedCartId == 0 || assignment.epoch == 0 ||
        assignment.state != RequestState::ASSIGNED)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (occupied_) {
      if (RequestIdentity::equals(current_.requestId, assignment.requestId) &&
          current_.assignedCartId == assignment.assignedCartId &&
          current_.epoch == assignment.epoch)
        return Status();
      return Status(ErrorCode::BUSY);
    }
    current_ = assignment;
    journey_ = JourneyStatus::RECEIVED;
    lastSequence_ = 0;
    occupied_ = true;
    return Status();
  }

  Status updateJourneyStatus(JourneyStatus status,
                             EventSequence sequence) override {
    if (!occupied_ || !isValidJourneyStatus(status))
      return Status(ErrorCode::INVALID_STATE);
    if (compareSequence(sequence, lastSequence_) != SerialNumberOrder::NEWER)
      return Status(ErrorCode::STALE_EVENT);
    if (!validStep(journey_, status))
      return Status(ErrorCode::INVALID_STATE);
    journey_ = status;
    lastSequence_ = sequence;
    current_.state = status == JourneyStatus::ON_THE_WAY
                         ? RequestState::ON_THE_WAY
                         : (status == JourneyStatus::ARRIVED_AT_PICKUP
                                ? RequestState::ARRIVED_AT_PICKUP
                                : (status == JourneyStatus::COMPLETED
                                       ? RequestState::COMPLETED
                                       : RequestState::ASSIGNED));
    return Status();
  }

  Status complete(const DriverAction& action) override {
    if (action.type != DriverActionType::CONFIRM_COMPLETION)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (!occupied_ || journey_ != JourneyStatus::ARRIVED_AT_PICKUP)
      return Status(ErrorCode::INVALID_STATE);
    journey_ = JourneyStatus::COMPLETED;
    current_.state = RequestState::COMPLETED;
    return Status();
  }

  Status clearAfterTerminal() override {
    if (!occupied_ || journey_ != JourneyStatus::COMPLETED)
      return Status(ErrorCode::INVALID_STATE);
    occupied_ = false;
    current_ = AssignmentRecord();
    journey_ = JourneyStatus::RECEIVED;
    lastSequence_ = 0;
    return Status();
  }

  bool get(AssignmentRecord& assignment) const override {
    if (!occupied_)
      return false;
    assignment = current_;
    return true;
  }

private:
  AssignmentRecord current_;
  bool occupied_;
  JourneyStatus journey_;
  EventSequence lastSequence_;

  static bool validStep(JourneyStatus current, JourneyStatus next) {
    return (current == JourneyStatus::RECEIVED &&
            next == JourneyStatus::ON_THE_WAY) ||
           (current == JourneyStatus::ON_THE_WAY &&
            next == JourneyStatus::ARRIVED_AT_PICKUP) ||
           (current == JourneyStatus::ARRIVED_AT_PICKUP &&
            next == JourneyStatus::COMPLETED);
  }
};

class DefaultJourneyProgressController final : public JourneyProgressController {
public:
  Status validateTransition(JourneyStatus current, JourneyStatus next,
                            const DriverAction& action) const override {
    if (!isValidJourneyStatus(current) || !isValidJourneyStatus(next))
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (current == JourneyStatus::RECEIVED &&
        next == JourneyStatus::ON_THE_WAY &&
        action.type == DriverActionType::ADVANCE_JOURNEY)
      return Status();
    if (current == JourneyStatus::ON_THE_WAY &&
        next == JourneyStatus::ARRIVED_AT_PICKUP &&
        action.type == DriverActionType::ADVANCE_JOURNEY)
      return Status();
    if (current == JourneyStatus::ARRIVED_AT_PICKUP &&
        next == JourneyStatus::COMPLETED &&
        action.type == DriverActionType::CONFIRM_COMPLETION)
      return Status();
    return Status(ErrorCode::INVALID_STATE);
  }
};

} // namespace ca
