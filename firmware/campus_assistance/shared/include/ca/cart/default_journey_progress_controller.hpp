#pragma once

#include "ca/domain/journey_progress_controller.hpp"

namespace ca {

// Stateless policy for validating driver-requested journey transitions.
class DefaultJourneyProgressController final : public JourneyProgressController {
public:
  Status validateTransition(JourneyStatus current,
                            JourneyStatus next,
                            const DriverAction& action) const override;
};

} // namespace ca
