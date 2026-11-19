#include "ca/protocol/fixed_retry_manager.hpp"

#include <assert.h>
#include <stdint.h>

using namespace ca;

static ProtocolMessage makeMessage(TransactionId transactionId) {
  ProtocolMessage message = {};
  message.header.protocolVersion = 1;
  message.header.networkId = 7;
  message.header.messageType = MessageType::REQUEST;
  message.header.sourceDeviceId = 1;
  message.header.destinationDeviceId = 0;
  message.header.transactionId = transactionId;
  message.header.messageSequence = 9;
  message.payloadLength = 2;
  message.payload[0] = 0x12;
  message.payload[1] = 0x34;
  return message;
}

int main() {
  const RetryPolicy policy = {3, 100, 400};
  FixedRetryManager<2> retries(policy);
  const ProtocolMessage message = makeMessage(77);

  assert(retries.schedule(message, 0).ok());
  assert(retries.schedule(message, 0).ok());
  assert(retries.pendingCount() == 1);

  ProtocolMessage due = {};
  uint8_t attempt = 0;
  retries.service(0);
  assert(retries.pollDue(due, attempt));
  assert(attempt == 1);
  assert(due.header.transactionId == 77);
  assert(retries.onTransmissionAttempt(77, true, 0).ok());

  retries.service(99);
  assert(!retries.pollDue(due, attempt));
  retries.service(100);
  assert(retries.pollDue(due, attempt));
  assert(attempt == 2);
  assert(retries.onTransmissionAttempt(77, false, 100).ok());

  retries.service(299);
  assert(!retries.pollDue(due, attempt));
  retries.service(300);
  assert(retries.pollDue(due, attempt));
  assert(attempt == 3);
  assert(retries.onTransmissionAttempt(77, true, 300).ok());
  assert(retries.pendingCount() == 0);

  RetryOutcome outcome = {};
  assert(retries.pollOutcome(outcome));
  assert(outcome.transactionId == 77);
  assert(outcome.outcome == RetryOutcomeCode::EXHAUSTED);
  assert(outcome.attemptsUsed == 3);
  assert(!retries.pollOutcome(outcome));

  assert(retries.schedule(makeMessage(88), 400).ok());
  retries.service(400);
  assert(retries.pollDue(due, attempt));
  assert(retries.onTransmissionAttempt(88, true, 400).ok());
  assert(retries.onAcknowledgement(88).ok());
  assert(retries.pollOutcome(outcome));
  assert(outcome.transactionId == 88);
  assert(outcome.outcome == RetryOutcomeCode::ACKNOWLEDGED);

  FixedRetryManager<1> rollover(policy);
  const uint32_t start = UINT32_MAX - 50U;
  assert(rollover.schedule(makeMessage(99), start).ok());
  rollover.service(start);
  assert(rollover.pollDue(due, attempt));
  assert(rollover.onTransmissionAttempt(99, true, start).ok());
  rollover.service(48U);
  assert(!rollover.pollDue(due, attempt));
  rollover.service(49U);
  assert(rollover.pollDue(due, attempt));
  assert(attempt == 2);
  return 0;
}
