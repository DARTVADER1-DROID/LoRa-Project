#include "CampusAssistance.h"
#include "ca/cart/fixed_cart_claim_client.hpp"

#include <assert.h>

using namespace ca;

int main() {
  FixedCartClaimClient client(7, 100);
  const RequestId request = {1, 22, 3};
  assert(client.requestClaim(request, 8).code == ErrorCode::INVALID_ARGUMENT);
  assert(client.requestClaim(request, 7).ok());
  assert(client.requestClaim(request, 7).code == ErrorCode::BUSY);

  ClaimRequest outstanding = {};
  assert(client.getOutstandingClaim(outstanding));
  assert(outstanding.cartId == 7);
  assert(outstanding.transactionId == 100);

  ClaimResult accepted = {};
  accepted.requestId = request;
  accepted.transactionId = 100;
  accepted.result = ErrorCode::NONE;
  accepted.finalState = ClaimState::PENDING;
  assert(client.onClaimResult(accepted).ok());
  assert(client.hasClaimInFlight());

  ClaimWithdraw withdrawal = {};
  withdrawal.requestId = request;
  withdrawal.cartId = 7;
  withdrawal.transactionId = 100;
  assert(client.withdrawClaim(withdrawal).ok());
  assert(client.withdrawClaim(withdrawal).ok());

  // A delayed result cannot revive a transaction after a confirmed assignment.
  AssignmentRecord assignment = {};
  assignment.requestId = request;
  assignment.assignedCartId = 7;
  assignment.epoch = 1;
  assignment.state = RequestState::ASSIGNED;
  assert(client.onAssignmentConfirmed(assignment).ok());
  assert(!client.hasClaimInFlight());
  assert(client.onClaimResult(accepted).code == ErrorCode::STALE_EVENT);

  const RequestId nextRequest = {1, 23, 3};
  assert(client.requestClaim(nextRequest, 7).ok());
  assert(client.getOutstandingClaim(outstanding));
  assert(outstanding.transactionId == 101);

  ClaimResult uncertain = {};
  uncertain.requestId = nextRequest;
  uncertain.transactionId = 101;
  uncertain.result = ErrorCode::STORAGE_FAILURE;
  uncertain.finalState = ClaimState::PENDING;
  assert(client.onClaimResult(uncertain).code == ErrorCode::COMMUNICATION_UNCERTAIN);
  assert(client.hasClaimInFlight());

  ClaimResult rejected = {};
  rejected.requestId = nextRequest;
  rejected.transactionId = 101;
  rejected.result = ErrorCode::BUSY;
  rejected.finalState = ClaimState::IDLE;
  assert(client.onClaimResult(rejected).ok());
  assert(!client.hasClaimInFlight());
  return 0;
}
