#include "ca/types.hpp"

#include <limits.h>

namespace ca {

SerialNumberOrder compareSequence(uint32_t candidate, uint32_t reference) {
  const uint32_t difference = candidate - reference;

  if (difference == 0)
    return SerialNumberOrder::EQUAL;

  const uint32_t halfRange = static_cast<uint32_t>(0x80000000UL);
  if (difference == halfRange)
    return SerialNumberOrder::AMBIGUOUS;

  return difference < halfRange ? SerialNumberOrder::NEWER
                                : SerialNumberOrder::OLDER;
}

bool hasElapsed(uint32_t nowMs, uint32_t startMs, uint32_t durationMs) {
  // Durations above half the counter range cannot be compared unambiguously.
  if (durationMs > static_cast<uint32_t>(INT32_MAX))
    return false;

  return static_cast<uint32_t>(nowMs - startMs) >= durationMs;
}

bool isTerminalRequestState(RequestState state) {
  switch (state) {
  case RequestState::COMPLETED:
  case RequestState::CANCELLED:
  case RequestState::EXPIRED:
    return true;

  case RequestState::CREATED:
  case RequestState::PENDING_UNASSIGNED:
  case RequestState::CLAIM_PENDING:
  case RequestState::ASSIGNED:
  case RequestState::ON_THE_WAY:
  case RequestState::ARRIVED_AT_PICKUP:
  case RequestState::COMMUNICATION_UNCERTAIN:
    return false;
  }

  return false;
}

bool isValidJourneyStatus(JourneyStatus status) {
  switch (status) {
  case JourneyStatus::RECEIVED:
  case JourneyStatus::ON_THE_WAY:
  case JourneyStatus::ARRIVED_AT_PICKUP:
  case JourneyStatus::COMPLETED:
    return true;
  }

  return false;
}

} // namespace ca
