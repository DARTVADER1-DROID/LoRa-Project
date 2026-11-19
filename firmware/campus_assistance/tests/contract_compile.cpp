#include "CampusAssistance.h"

#include <stddef.h>
#include <type_traits>

using namespace ca;

static_assert(kDefaultRequestLifetimeMs == 600000UL,
              "The default unassigned-request lifetime is 10 minutes.");
static_assert(kMaxPhysicalButtons == 2,
              "The physical interface is limited to two buttons.");
static_assert(sizeof(((UiViewModel*)0)->line1) == 17,
              "The LCD line buffer includes 16 characters and a terminator.");
static_assert(static_cast<uint8_t>(AcceptanceMode::NOT_ACCEPTING) !=
                  static_cast<uint8_t>(AcceptanceMode::ACCEPTING),
              "Acceptance is an independent state dimension.");
static_assert(static_cast<uint8_t>(ClaimState::PENDING) !=
                  static_cast<uint8_t>(ClaimState::WITHDRAWN),
              "A withdrawn claim must be distinguishable from a pending claim.");

static_assert(std::is_abstract<IRequestRepository>::value,
              "Repository behavior must be supplied by a concrete adapter.");
static_assert(std::is_abstract<StationClaimCoordinator>::value,
              "Station claim coordination must be supplied by a concrete service.");
static_assert(std::is_abstract<ProtocolEndpoint>::value,
              "Protocol endpoint behavior must be supplied by a concrete service.");

int main() {
  RequestRecord record = {};
  record.claimState = ClaimState::IDLE;
  record.state = RequestState::PENDING_UNASSIGNED;
  return (record.claimState == ClaimState::IDLE &&
          record.state == RequestState::PENDING_UNASSIGNED)
             ? 0
             : 1;
}
