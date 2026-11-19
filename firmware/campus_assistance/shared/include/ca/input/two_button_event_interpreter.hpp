#pragma once

#include "ca/ui.hpp"

namespace ca {

// Reference two-button mapping. The constructor fixes the device role; button
// indices are logical profile indices, not hard-coded GPIO numbers.
class TwoButtonEventInterpreter final : public ButtonEventInterpreter {
public:
  explicit TwoButtonEventInterpreter(DeviceRole role) : role_(role) {}

  UiIntent interpret(const ButtonEvent& event,
                     const UiContext& context) const override;

private:
  DeviceRole role_;
};

} // namespace ca
