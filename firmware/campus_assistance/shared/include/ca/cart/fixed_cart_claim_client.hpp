#pragma once

#include "ca/domain/cart_claim_client.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

// Allocation-free cart claim owner. initialTransactionId must come from a
// durable per-cart allocator so transaction identities are not reused on reboot.
class FixedCartClaimClient final : public CartClaimClient {
public:
  FixedCartClaimClient(DeviceId cartId, TransactionId initialTransactionId);

  Status requestClaim(const RequestId& id, DeviceId cartId) override;
  Status withdrawClaim(const ClaimWithdraw& withdrawal) override;
  Status onClaimResult(const ClaimResult& result) override;
  Status onAssignmentConfirmed(const AssignmentRecord& assignment) override;
  bool hasClaimInFlight() const override;
  bool getOutstandingClaim(ClaimRequest& claim) const override;

private:
  DeviceId cartId_;
  TransactionId nextTransactionId_;
  bool exhausted_;
  bool inFlight_;
  bool withdrawalRequested_;
  ClaimRequest outstanding_;

  void clear();
};

} // namespace ca
