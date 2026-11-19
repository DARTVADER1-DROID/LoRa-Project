#include "ca/cart/fixed_active_assignment.hpp"

namespace ca {

FixedActiveAssignment::FixedActiveAssignment()
    : current_(), occupied_(false), journey_(JourneyStatus::RECEIVED),
      lastSequence_(0) {
}

bool FixedActiveAssignment::isOccupied() const {
  return occupied_;
}

Status FixedActiveAssignment::activate(const AssignmentRecord& assignment) {
  if (!RequestIdentity::isValid(assignment.requestId) ||
      assignment.assignedCartId == 0 || assignment.epoch == 0 ||
      assignment.state != RequestState::ASSIGNED || assignment.lastEventSequence != 0)
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
  current_.lastEventSequence = 0;
  occupied_ = true;
  return Status();
}

Status FixedActiveAssignment::updateJourneyStatus(JourneyStatus status,
                                                  EventSequence sequence) {
  if (!occupied_ || !isValidJourneyStatus(status))
    return Status(ErrorCode::INVALID_STATE);
  if (compareSequence(sequence, lastSequence_) != SerialNumberOrder::NEWER)
    return Status(ErrorCode::STALE_EVENT);
  if (status != journey_ && !validStep(journey_, status))
    return Status(ErrorCode::INVALID_STATE);

  // A newer authoritative echo of a locally completed/advanced state is
  // idempotent; it may advance the event cursor but never regress the journey.
  journey_ = status;
  lastSequence_ = sequence;
  current_.lastEventSequence = sequence;
  current_.state =
      status == JourneyStatus::ON_THE_WAY
          ? RequestState::ON_THE_WAY
          : (status == JourneyStatus::ARRIVED_AT_PICKUP
                 ? RequestState::ARRIVED_AT_PICKUP
                 : (status == JourneyStatus::COMPLETED ? RequestState::COMPLETED
                                                       : RequestState::ASSIGNED));
  return Status();
}

Status FixedActiveAssignment::reconcileSnapshot(JourneyStatus status,
                                                EventSequence sequence) {
  if (!occupied_ || !isValidJourneyStatus(status))
    return Status(ErrorCode::INVALID_STATE);
  if (compareSequence(sequence, lastSequence_) != SerialNumberOrder::NEWER)
    return Status(ErrorCode::STALE_EVENT);
  if (static_cast<uint8_t>(status) < static_cast<uint8_t>(journey_))
    return Status(ErrorCode::INVALID_STATE);

  journey_ = status;
  lastSequence_ = sequence;
  current_.lastEventSequence = sequence;
  current_.state =
      status == JourneyStatus::ON_THE_WAY
          ? RequestState::ON_THE_WAY
          : (status == JourneyStatus::ARRIVED_AT_PICKUP
                 ? RequestState::ARRIVED_AT_PICKUP
                 : (status == JourneyStatus::COMPLETED ? RequestState::COMPLETED
                                                       : RequestState::ASSIGNED));
  return Status();
}

Status FixedActiveAssignment::complete(const DriverAction& action) {
  if (action.type != DriverActionType::CONFIRM_COMPLETION)
    return Status(ErrorCode::INVALID_ARGUMENT);
  if (!occupied_ || journey_ != JourneyStatus::ARRIVED_AT_PICKUP)
    return Status(ErrorCode::INVALID_STATE);

  journey_ = JourneyStatus::COMPLETED;
  current_.state = RequestState::COMPLETED;
  return Status();
}

Status FixedActiveAssignment::clearAfterTerminal() {
  if (!occupied_ || journey_ != JourneyStatus::COMPLETED)
    return Status(ErrorCode::INVALID_STATE);

  occupied_ = false;
  current_ = AssignmentRecord();
  journey_ = JourneyStatus::RECEIVED;
  lastSequence_ = 0;
  return Status();
}

bool FixedActiveAssignment::get(AssignmentRecord& assignment) const {
  if (!occupied_)
    return false;
  assignment = current_;
  return true;
}

bool FixedActiveAssignment::validStep(JourneyStatus current, JourneyStatus next) {
  return (current == JourneyStatus::RECEIVED && next == JourneyStatus::ON_THE_WAY) ||
         (current == JourneyStatus::ON_THE_WAY &&
          next == JourneyStatus::ARRIVED_AT_PICKUP) ||
         (current == JourneyStatus::ARRIVED_AT_PICKUP &&
          next == JourneyStatus::COMPLETED);
}

} // namespace ca
