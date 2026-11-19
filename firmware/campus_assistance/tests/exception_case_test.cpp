#include "ca/protocol/fixed_retry_manager.hpp"

#include <stdint.h>

// Return the failing source line instead of opening the MSVC assertion dialog.
#define assert(condition) do { if (!(condition)) return __LINE__; } while (0)

using namespace ca;

static ProtocolMessage message(TransactionId transactionId,
                               uint32_t sequence,
                               DeviceId destination = 2) {
  ProtocolMessage result = {};
  result.header.protocolVersion = 1;
  result.header.networkId = 7;
  result.header.messageType = MessageType::ASSIGN;
  result.header.sourceDeviceId = 1;
  result.header.destinationDeviceId = destination;
  result.header.transactionId = transactionId;
  result.header.messageSequence = sequence;
  result.payloadLength = 1;
  result.payload[0] = 0xA5;
  return result;
}

int main() {
  const RetryPolicy validPolicy = {2, 10, 40};
  ProtocolMessage due = {};
  uint8_t attempt = 0;
  RetryOutcome outcome = {};

  // Invalid policy and invalid transaction identity must fail closed.
  FixedRetryManager<2> zeroAttempts(RetryPolicy{0, 10, 40});
  assert(zeroAttempts.schedule(message(1, 1), 0).code ==
         ErrorCode::INVALID_ARGUMENT);
  FixedRetryManager<2> zeroBackoff(RetryPolicy{2, 0, 40});
  assert(zeroBackoff.schedule(message(1, 1), 0).code ==
         ErrorCode::INVALID_ARGUMENT);
  FixedRetryManager<2> invertedBackoff(RetryPolicy{2, 20, 10});
  assert(invertedBackoff.schedule(message(1, 1), 0).code ==
         ErrorCode::INVALID_ARGUMENT);
  FixedRetryManager<2> excessiveBackoff(
      RetryPolicy{2, 1, 0x80000000UL});
  assert(excessiveBackoff.schedule(message(1, 1), 0).code ==
         ErrorCode::INVALID_ARGUMENT);
  FixedRetryManager<2> invalidIdentity(validPolicy);
  assert(invalidIdentity.schedule(message(0, 1), 0).code ==
         ErrorCode::INVALID_ARGUMENT);

  // Duplicate enqueue is idempotent; identity reuse with changed content is not.
  FixedRetryManager<2> manager(validPolicy);
  ProtocolMessage first = message(10, 100);
  assert(manager.schedule(first, 0).ok());
  assert(manager.schedule(first, 1).ok());
  ProtocolMessage conflicting = first;
  conflicting.payload[0] ^= 0x01;
  assert(manager.schedule(conflicting, 1).code == ErrorCode::INVALID_STATE);

  // Capacity exhaustion is explicit and does not evict active delivery.
  assert(manager.schedule(message(11, 101), 1).ok());
  assert(manager.schedule(message(12, 102), 1).code == ErrorCode::QUEUE_FULL);
  assert(manager.pendingCount() == 2);

  // ACKs cannot complete a message before a transmission attempt.
  assert(manager.onAcknowledgement(10, 2, 100).code == ErrorCode::STALE_EVENT);
  assert(manager.pollDue(due, attempt));
  assert(due.header.transactionId == 10);
  assert(attempt == 1);
  assert(manager.onTransmissionAttempt(999, 100, true, 2).code ==
         ErrorCode::NOT_FOUND);
  assert(manager.onTransmissionAttempt(10, 999, true, 2).code ==
         ErrorCode::NOT_FOUND);
  assert(manager.onTransmissionAttempt(10, 100, true, 2).ok());
  assert(manager.pollDue(due, attempt));
  assert(due.header.transactionId == 11);
  assert(manager.onTransmissionAttempt(11, 101, true, 2).ok());

  // An early poll cannot bypass the configured backoff.
  assert(!manager.pollDue(due, attempt));
  assert(manager.onAcknowledgement(10, 99, 100).code == ErrorCode::STALE_EVENT);
  assert(manager.onAcknowledgement(10, 2, 999).code == ErrorCode::STALE_EVENT);
  assert(manager.onAcknowledgement(10, 2, 100).ok());
  assert(manager.pollOutcome(outcome));
  assert(outcome.transactionId == 10);
  assert(outcome.outcome == RetryOutcomeCode::ACKNOWLEDGED);
  assert(outcome.attemptsUsed == 1);
  assert(!manager.pollOutcome(outcome));

  // Cancelling an in-flight delivery is observable exactly once.
  assert(manager.cancel(11, 101).ok());
  assert(manager.cancel(11, 101).code == ErrorCode::NOT_FOUND);
  manager.service(3);
  assert(manager.pollOutcome(outcome));
  assert(outcome.transactionId == 11);
  assert(outcome.outcome == RetryOutcomeCode::CANCELLED);
  assert(outcome.attemptsUsed == 1);
  assert(manager.onAcknowledgement(999, 2, 1).code == ErrorCode::NOT_FOUND);

  // Exhaustion emits a terminal outcome after exactly maxAttempts attempts.
  FixedRetryManager<1> exhausted(RetryPolicy{2, 10, 20});
  assert(exhausted.schedule(message(20, 200), 100).ok());
  assert(exhausted.pollDue(due, attempt) && attempt == 1);
  assert(exhausted.onTransmissionAttempt(20, 200, false, 100).ok());
  assert(!exhausted.pollDue(due, attempt));
  exhausted.service(109);
  assert(!exhausted.pollOutcome(outcome));
  assert(exhausted.pollDue(due, attempt) == false);
  assert(exhausted.pollDue(due, attempt) == false);
  // Backoff expires at 110; rejected submissions still consume an attempt.
  exhausted.service(110);
  assert(exhausted.pollDue(due, attempt) && attempt == 2);
  assert(exhausted.onTransmissionAttempt(20, 200, false, 110).ok());
  exhausted.service(110);
  assert(exhausted.pollOutcome(outcome));
  assert(outcome.outcome == RetryOutcomeCode::EXHAUSTED);
  assert(outcome.attemptsUsed == 2);
  assert(exhausted.pendingCount() == 0);

  // Unsigned clock rollover must preserve retry timing.
  FixedRetryManager<1> rollover(RetryPolicy{2, 10, 20});
  assert(rollover.schedule(message(30, 300), 0xFFFFFFF8UL).ok());
  assert(rollover.pollDue(due, attempt) && attempt == 1);
  assert(rollover.onTransmissionAttempt(30, 300, true, 0xFFFFFFF8UL).ok());
  rollover.service(0xFFFFFFFFUL);
  assert(!rollover.pollDue(due, attempt));
  rollover.service(0x00000000UL);
  assert(!rollover.pollDue(due, attempt));
  rollover.service(0x00000001UL);
  assert(!rollover.pollDue(due, attempt));
  rollover.service(0x00000002UL);
  assert(rollover.pollDue(due, attempt) && attempt == 2);

  return 0;
}
