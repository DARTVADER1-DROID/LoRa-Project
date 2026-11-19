#pragma once

#include "ca/domain/driver_availability_controller.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

// Concrete cart policy. Claim transaction identity remains owned by
// CartClaimClient; this controller owns only acceptance/service state.
class BasicDriverAvailabilityController final : public DriverAvailabilityController {
public:
  BasicDriverAvailabilityController();

  AcceptanceMode acceptanceMode() const override;
  CartServiceState serviceState() const override;
  Status enableAcceptance(const DriverAction& action) override;
  Status disableAcceptance(const DriverAction& action) override;
  Status onAssignmentConfirmed(const AssignmentRecord& assignment) override;
  Status onJourneyCompleted() override;
  bool mayClaim() const override;
  Status onClaimStarted() override;
  Status onClaimWithdrawnByStation() override;

private:
  AcceptanceMode acceptance_;
  CartServiceState service_;
};

} // namespace ca
