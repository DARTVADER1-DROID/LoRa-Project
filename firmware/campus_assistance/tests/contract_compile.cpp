#include "CampusAssistance.h"

// Compile-time checks for repository, claim and assignment ownership boundaries.

#include <stddef.h>
#include <type_traits>

using namespace ca;

template <typename T> class HasGenericUpdate {
private:
  template <typename U> static auto test(int) -> decltype(&U::update, std::true_type());
  template <typename> static std::false_type test(...);

public:
  static const bool value = decltype(test<T>(0))::value;
};

template <typename T> class HasAssignmentNoticeHandler {
private:
  template <typename U>
  static auto test(int) -> decltype(&U::onAssignmentNotice, std::true_type());
  template <typename> static std::false_type test(...);

public:
  static const bool value = decltype(test<T>(0))::value;
};

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
static_assert(!std::is_abstract<DurableRequestRepository<2>>::value,
              "The durable repository must implement the complete repository port.");
static_assert(!std::is_abstract<StationRequestLifecycle>::value,
              "The station lifecycle must implement the domain contract.");
static_assert(!HasGenericUpdate<IRequestRepository>::value,
              "Generic record updates must not bypass atomic state transitions.");
static_assert(!HasAssignmentNoticeHandler<CartClaimClient>::value,
              "Claim tracking must not duplicate assignment ownership.");
static_assert(std::is_abstract<StationClaimCoordinator>::value,
              "Station claim coordination must be supplied by a concrete service.");
static_assert(std::is_abstract<ProtocolEndpoint>::value,
              "Protocol endpoint behavior must be supplied by a concrete service.");

int main() {
  RequestRecord record = {};
  record.claimState = ClaimState::IDLE;
  record.state = RequestState::PENDING_UNASSIGNED;
  record.claimTransactionId = 0;
  return (record.claimState == ClaimState::IDLE &&
          record.state == RequestState::PENDING_UNASSIGNED)
             ? 0
             : 1;
}
