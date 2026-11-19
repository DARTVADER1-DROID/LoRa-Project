#pragma once

#include "ca/domain.hpp"

namespace ca {

// Stateless policy for validating driver-requested journey transitions.
class DefaultJourneyProgressController final : public JourneyProgressController {
public:
  Status validateTransition(JourneyStatus current,
                            JourneyStatus next,
                            const DriverAction& action) const override;
};

} // namespace ca
