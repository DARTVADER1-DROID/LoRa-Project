#pragma once

#include "ca/types.hpp"

namespace ca {

// Stateless transition policy. ActiveAssignment owns current journey state.
class JourneyProgressController {
public:
  virtual ~JourneyProgressController() {}
  virtual Status validateTransition(JourneyStatus current,
                                    JourneyStatus next,
                                    const DriverAction& action) const = 0;
};

} // namespace ca
