#pragma once

#include "ca/types.hpp"

namespace ca {

// Applies authoritative notices to domain owners without retaining duplicate state.
class EventReconciler {
public:
  virtual ~EventReconciler() {}
  virtual Status applyAssignmentNotice(const AssignmentNotice& notice) = 0;
  virtual Status applyCancellationNotice(const CancellationNotice& notice) = 0;
  virtual Status applyExpiryNotice(const ExpiryNotice& notice) = 0;
  virtual Status applySnapshot(const RequestSnapshot& snapshot) = 0;
};

} // namespace ca
