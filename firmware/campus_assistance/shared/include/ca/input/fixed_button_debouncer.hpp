#pragma once

#include "ca/types.hpp"

namespace ca {

// Portable debounce/gesture engine. A board adapter samples GPIO and feeds
// active-high logical pressed states; no Arduino dependency is introduced.
class FixedButtonDebouncer {
public:
  FixedButtonDebouncer();

  Status configure(const ButtonConfig& config);
  Status update(const bool* pressed, uint8_t count, uint32_t nowMs);
  bool pollEvent(ButtonEvent& event);
  bool isConfigured() const;

private:
  static const uint8_t kEventCapacity = 8;

  struct ButtonState {
    bool initialized;
    bool rawPressed;
    bool stablePressed;
    bool armed;
    bool longReported;
    uint32_t rawChangedAtMs;
    uint32_t pressedAtMs;
  };

  ButtonConfig config_;
  ButtonState buttons_[kMaxPhysicalButtons];
  ButtonEvent events_[kEventCapacity];
  uint8_t eventHead_;
  uint8_t eventCount_;
  bool configured_;

  Status enqueue(uint8_t buttonIndex, ButtonEventType type, uint32_t nowMs);
  Status updateButton(uint8_t index, bool pressed, uint32_t nowMs);
};

} // namespace ca
