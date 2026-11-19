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

  AssignmentRecord assignment = {};
  assignment.requestId.originStationId = 1;
  assignment.requestId.sequence = 2;
  assignment.requestId.generation = 1;
  assignment.assignedCartId = 7;
  assignment.epoch = 1;
  assignment.state = RequestState::ASSIGNED;
  assert(driver.onAssignmentConfirmed(assignment).ok());
  assert(driver.onJourneyCompleted().ok());

  FixedActiveAssignment active;
  assert(active.activate(assignment).ok());
  assert(active.activate(assignment).ok());
  AssignmentRecord other = assignment;
  other.requestId.sequence = 3;
  assert(active.activate(other).code == ErrorCode::BUSY);
  assert(active.updateJourneyStatus(JourneyStatus::ARRIVED_AT_PICKUP, 1).code ==
         ErrorCode::INVALID_STATE);
  assert(active.updateJourneyStatus(JourneyStatus::ON_THE_WAY, 1).ok());
  AssignmentRecord current = {};
  assert(active.get(current));
  assert(current.lastEventSequence == 1);
  assert(active.updateJourneyStatus(JourneyStatus::ON_THE_WAY, 1).code ==
         ErrorCode::STALE_EVENT);
  assert(active.updateJourneyStatus(JourneyStatus::ARRIVED_AT_PICKUP, 2).ok());
  assert(active.get(current));
  assert(current.lastEventSequence == 2);

  DriverAction complete = {DriverActionType::CONFIRM_COMPLETION, 3};
  assert(active.complete(complete).ok());
  // A newer station echo may confirm local completion without regressing state.
  assert(active.updateJourneyStatus(JourneyStatus::COMPLETED, 3).ok());
  assert(active.get(current));
  assert(current.lastEventSequence == 3);
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

  FixedActiveAssignment recovered;
  assert(recovered.activate(assignment).ok());
  assert(recovered.reconcileSnapshot(JourneyStatus::ON_THE_WAY, 0xFFFFFFFFUL).ok());
  assert(recovered.reconcileSnapshot(JourneyStatus::ARRIVED_AT_PICKUP, 0).ok());
  assert(recovered.get(current));
  assert(current.lastEventSequence == 0);
  return 0;
}
