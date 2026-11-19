#pragma once

#include "ca/types.hpp"

namespace ca {

// Sole station-side claim transaction and assignment-commit authority.
class StationClaimCoordinator {
public:
  virtual ~StationClaimCoordinator() {}
  virtual Status evaluate(const ClaimRequest& claim, uint32_t stationNowMs) const = 0;
  virtual Status withdrawClaim(const ClaimWithdraw& withdrawal) = 0;
  virtual Status commitWinner(const ClaimRequest& claim,
                              uint32_t stationNowMs,
                              AssignmentRecord& assignment) = 0;
  virtual bool isAssigned(const RequestId& id) const = 0;
};

} // namespace ca
