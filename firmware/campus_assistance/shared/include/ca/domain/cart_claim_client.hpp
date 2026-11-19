#pragma once

#include "ca/types.hpp"

namespace ca {

// Cart-side claim transaction only. It never activates an assignment.
class CartClaimClient {
public:
  virtual ~CartClaimClient() {}
  virtual Status requestClaim(const RequestId& id, DeviceId cartId) = 0;
  virtual Status withdrawClaim(const ClaimWithdraw& withdrawal) = 0;
  virtual Status onClaimResult(const ClaimResult& result) = 0;
  virtual Status onAssignmentConfirmed(const AssignmentRecord& assignment) = 0;
  virtual bool hasClaimInFlight() const = 0;
  virtual bool getOutstandingClaim(ClaimRequest& claim) const = 0;
};

} // namespace ca
