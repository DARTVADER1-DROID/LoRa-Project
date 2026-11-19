#pragma once

#include "ca/domain/station_claim_coordinator.hpp"
#include "ca/ports/request_repository.hpp"

namespace ca {

// Concrete station arbitration adapter. Repository transitions are the
// serialization point for competing claims, withdrawals and assignment commits.
class RepositoryStationClaimCoordinator final : public StationClaimCoordinator {
public:
  explicit RepositoryStationClaimCoordinator(IRequestRepository& repository);

  Status evaluate(const ClaimRequest& claim, uint32_t stationNowMs) override;
  Status withdrawClaim(const ClaimWithdraw& withdrawal) override;
  Status commitWinner(const ClaimRequest& claim,
                      uint32_t stationNowMs,
                      AssignmentRecord& assignment) override;
  bool isAssigned(const RequestId& id) const override;
  bool getAssignment(const RequestId& id, AssignmentRecord& assignment) const override;

private:
  IRequestRepository& repository_;
};

} // namespace ca
