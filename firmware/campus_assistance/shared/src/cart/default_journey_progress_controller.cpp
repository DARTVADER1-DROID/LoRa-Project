#include "ca/cart/default_journey_progress_controller.hpp"

namespace ca {

Status DefaultJourneyProgressController::validateTransition(
    JourneyStatus current, JourneyStatus next, const DriverAction& action) const {
  if (!isValidJourneyStatus(current) || !isValidJourneyStatus(next))
    return Status(ErrorCode::INVALID_ARGUMENT);

  if (current == JourneyStatus::RECEIVED && next == JourneyStatus::ON_THE_WAY &&
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

} // namespace ca
