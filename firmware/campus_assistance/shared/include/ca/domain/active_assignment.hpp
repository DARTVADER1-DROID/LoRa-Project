#pragma once

#include "ca/types.hpp"

namespace ca {

class ActiveAssignment {
public:
  virtual ~ActiveAssignment() {}
  virtual bool isOccupied() const = 0;
  virtual Status activate(const AssignmentRecord& stationConfirmed) = 0;
  virtual Status updateJourneyStatus(JourneyStatus status, EventSequence sequence) = 0;
  virtual Status complete(const DriverAction& action) = 0;
  virtual Status clearAfterTerminal() = 0;
  virtual bool get(AssignmentRecord& assignment) const = 0;
};

} // namespace ca
