#pragma once

#include "ca/types.hpp"

namespace ca {

class DriverAvailabilityController {
public:
  virtual ~DriverAvailabilityController() {}
  virtual AcceptanceMode acceptanceMode() const = 0;
  virtual CartServiceState serviceState() const = 0;
  virtual Status enableAcceptance(const DriverAction& action) = 0;
  virtual Status disableAcceptance(const DriverAction& action) = 0;
  virtual Status onAssignmentConfirmed(const AssignmentRecord& assignment) = 0;
  virtual Status onJourneyCompleted() = 0;
  virtual bool mayClaim() const = 0;
  virtual Status onClaimStarted() = 0;
  virtual Status onClaimWithdrawnByStation() = 0;
};

} // namespace ca
