#pragma once

#include "ca/domain/active_assignment.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

// Sole cart-side owner of active assignment and journey state.
class FixedActiveAssignment final : public ActiveAssignment {
public:
  FixedActiveAssignment();

  bool isOccupied() const override;
  Status activate(const AssignmentRecord& stationConfirmed) override;
  Status updateJourneyStatus(JourneyStatus status, EventSequence sequence) override;
  Status complete(const DriverAction& action) override;
  Status clearAfterTerminal() override;
  bool get(AssignmentRecord& assignment) const override;

private:
  AssignmentRecord current_;
  bool occupied_;
  JourneyStatus journey_;
  EventSequence lastSequence_;

  static bool validStep(JourneyStatus current, JourneyStatus next);
};

} // namespace ca
