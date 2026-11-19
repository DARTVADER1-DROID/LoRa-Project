#include "CampusAssistance.h"

#include <assert.h>

using namespace ca;

int main() {
  BasicDriverAvailabilityController driver;
  DriverAction enable = {DriverActionType::ENABLE_ACCEPTANCE, 1};
  DriverAction disable = {DriverActionType::DISABLE_ACCEPTANCE, 2};

  assert(driver.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);
  assert(!driver.mayClaim());
  assert(driver.enableAcceptance(enable).ok());
  assert(driver.mayClaim());
  assert(driver.onClaimStarted().ok());
  assert(driver.disableAcceptance(disable).code == ErrorCode::COMMUNICATION_UNCERTAIN);
  assert(driver.disableAcceptance(disable).code == ErrorCode::COMMUNICATION_UNCERTAIN);
  assert(driver.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);
  assert(driver.onClaimWithdrawnByStation().ok());
  assert(driver.serviceState() == CartServiceState::IDLE);
  assert(driver.onJourneyCompleted().ok());

  AssignmentRecord assignment = {};
  assignment.requestId.originStationId = 1;
  assignment.requestId.sequence = 2;
  assignment.requestId.generation = 1;
  assignment.assignedCartId = 7;
  assignment.epoch = 1;
  assignment.state = RequestState::ASSIGNED;

  FixedActiveAssignment active;
  assert(active.activate(assignment).ok());
  assert(active.activate(assignment).ok());
  AssignmentRecord other = assignment;
  other.requestId.sequence = 3;
  assert(active.activate(other).code == ErrorCode::BUSY);
  assert(active.updateJourneyStatus(JourneyStatus::ARRIVED_AT_PICKUP, 1).code ==
         ErrorCode::INVALID_STATE);
  assert(active.updateJourneyStatus(JourneyStatus::ON_THE_WAY, 1).ok());
  assert(active.updateJourneyStatus(JourneyStatus::ON_THE_WAY, 1).code ==
         ErrorCode::STALE_EVENT);
  assert(active.updateJourneyStatus(JourneyStatus::ARRIVED_AT_PICKUP, 2).ok());

  DriverAction complete = {DriverActionType::CONFIRM_COMPLETION, 3};
  assert(active.complete(complete).ok());
  assert(active.updateJourneyStatus(JourneyStatus::COMPLETED, 3).ok());
  assert(active.updateJourneyStatus(JourneyStatus::COMPLETED, 3).code ==
         ErrorCode::STALE_EVENT);
  assert(active.clearAfterTerminal().ok());
  assert(!active.isOccupied());

  DefaultJourneyProgressController policy;
  DriverAction advance = {DriverActionType::ADVANCE_JOURNEY, 4};
  assert(policy
             .validateTransition(
                 JourneyStatus::RECEIVED, JourneyStatus::ON_THE_WAY, advance)
             .ok());
  assert(policy
             .validateTransition(
                 JourneyStatus::RECEIVED, JourneyStatus::COMPLETED, complete)
             .code == ErrorCode::INVALID_STATE);
  return 0;
}
