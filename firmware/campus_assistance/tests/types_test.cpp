#include "ca/types.hpp"

#include <assert.h>
#include <limits.h>

using namespace ca;

void testSequenceComparison() {
  assert(compareSequence(10, 10) == SerialNumberOrder::EQUAL);
  assert(compareSequence(11, 10) == SerialNumberOrder::NEWER);
  assert(compareSequence(9, 10) == SerialNumberOrder::OLDER);

  // Sequence rollover is ordered using unsigned serial-number arithmetic.
  assert(compareSequence(0, UINT32_MAX) == SerialNumberOrder::NEWER);
  assert(compareSequence(UINT32_MAX, 0) == SerialNumberOrder::OLDER);

  // Exactly half a range apart has no unambiguous ordering.
  assert(compareSequence(0x80000000UL, 0) == SerialNumberOrder::AMBIGUOUS);
  assert(compareSequence(0, 0x80000000UL) == SerialNumberOrder::AMBIGUOUS);
}

void testElapsedTime() {
  assert(hasElapsed(100, 90, 10));
  assert(!hasElapsed(99, 90, 10));
  assert(hasElapsed(90, 90, 0));

  // Millisecond counter rollover must not break elapsed-time checks.
  assert(hasElapsed(5, UINT32_MAX - 4UL, 10));
  assert(!hasElapsed(4, UINT32_MAX - 4UL, 10));

  assert(hasElapsed(0x7FFFFFFFUL, 0, 0x7FFFFFFFUL));
  assert(!hasElapsed(0x80000000UL, 0, 0x80000000UL));
}

void testTerminalRequestStates() {
  assert(isTerminalRequestState(RequestState::COMPLETED));
  assert(isTerminalRequestState(RequestState::CANCELLED));
  assert(isTerminalRequestState(RequestState::EXPIRED));

  assert(!isTerminalRequestState(RequestState::CREATED));
  assert(!isTerminalRequestState(RequestState::PENDING_UNASSIGNED));
  assert(!isTerminalRequestState(RequestState::CLAIM_PENDING));
  assert(!isTerminalRequestState(RequestState::ASSIGNED));
  assert(!isTerminalRequestState(RequestState::ON_THE_WAY));
  assert(!isTerminalRequestState(RequestState::ARRIVED_AT_PICKUP));

  // Communication uncertainty is unresolved, not terminal.
  assert(!isTerminalRequestState(RequestState::COMMUNICATION_UNCERTAIN));
  assert(!isTerminalRequestState(static_cast<RequestState>(0xFF)));
}

void testJourneyStatusValidity() {
  assert(isValidJourneyStatus(JourneyStatus::RECEIVED));
  assert(isValidJourneyStatus(JourneyStatus::ON_THE_WAY));
  assert(isValidJourneyStatus(JourneyStatus::ARRIVED_AT_PICKUP));
  assert(isValidJourneyStatus(JourneyStatus::COMPLETED));
  assert(!isValidJourneyStatus(static_cast<JourneyStatus>(0xFF)));
}

int main() {
  testSequenceComparison();
  testElapsedTime();
  testTerminalRequestStates();
  testJourneyStatusValidity();
  return 0;
}
