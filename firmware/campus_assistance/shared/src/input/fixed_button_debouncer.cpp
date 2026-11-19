#include "ca/input/fixed_button_debouncer.hpp"

namespace ca {

FixedButtonDebouncer::FixedButtonDebouncer()
    : config_(), buttons_(), events_(), eventHead_(0), eventCount_(0),
      configured_(false) {
}

Status FixedButtonDebouncer::configure(const ButtonConfig& config) {
  if (config.buttonCount == 0 || config.buttonCount > kMaxPhysicalButtons ||
      config.debounceMs == 0 || config.longPressMs <= config.debounceMs)
    return Status(ErrorCode::INVALID_ARGUMENT);
  if (configured_)
    return Status(ErrorCode::INVALID_STATE);

  config_ = config;
  for (uint8_t i = 0; i < kMaxPhysicalButtons; ++i)
    buttons_[i] = ButtonState();
  eventHead_ = 0;
  eventCount_ = 0;
  configured_ = true;
  return Status();
}

Status
FixedButtonDebouncer::update(const bool* pressed, uint8_t count, uint32_t nowMs) {
  if (!configured_)
    return Status(ErrorCode::INVALID_STATE);
  if (pressed == 0 || count != config_.buttonCount)
    return Status(ErrorCode::INVALID_ARGUMENT);

  for (uint8_t i = 0; i < count; ++i) {
    const Status status = updateButton(i, pressed[i], nowMs);
    if (!status.ok())
      return status;
  }
  return Status();
}

bool FixedButtonDebouncer::pollEvent(ButtonEvent& event) {
  if (eventCount_ == 0)
    return false;
  event = events_[eventHead_];
  eventHead_ = static_cast<uint8_t>((eventHead_ + 1) % kEventCapacity);
  --eventCount_;
  return true;
}

bool FixedButtonDebouncer::isConfigured() const {
  return configured_;
}

Status FixedButtonDebouncer::enqueue(uint8_t buttonIndex,
                                     ButtonEventType type,
                                     uint32_t nowMs) {
  if (eventCount_ >= kEventCapacity)
    return Status(ErrorCode::BUFFER_FULL);
  const uint8_t tail =
      static_cast<uint8_t>((eventHead_ + eventCount_) % kEventCapacity);
  events_[tail].buttonIndex = buttonIndex;
  events_[tail].type = type;
  events_[tail].timestampMs = nowMs;
  ++eventCount_;
  return Status();
}

Status FixedButtonDebouncer::updateButton(uint8_t index, bool pressed, uint32_t nowMs) {
  ButtonState& state = buttons_[index];
  if (!state.initialized) {
    state.initialized = true;
    state.rawPressed = pressed;
    state.stablePressed = pressed;
    state.armed = !pressed;
    state.rawChangedAtMs = nowMs;
    state.pressedAtMs = nowMs;
    return Status();
  }

  if (pressed != state.rawPressed) {
    state.rawPressed = pressed;
    state.rawChangedAtMs = nowMs;
  }

  if (state.rawPressed != state.stablePressed &&
      hasElapsed(nowMs, state.rawChangedAtMs, config_.debounceMs)) {
    if (state.stablePressed && !state.rawPressed && state.armed) {
      const uint8_t requiredEvents = state.longReported ? 1 : 2;
      if (eventCount_ > kEventCapacity - requiredEvents)
        return Status(ErrorCode::BUFFER_FULL);
    }
    state.stablePressed = state.rawPressed;
    if (state.stablePressed) {
      if (state.armed) {
        state.pressedAtMs = nowMs;
        state.longReported = false;
      }
    } else if (state.armed) {
      Status status;
      if (!state.longReported) {
        status = enqueue(index, ButtonEventType::SHORT_PRESS, nowMs);
        if (!status.ok())
          return status;
      }
      status = enqueue(index, ButtonEventType::RELEASE, nowMs);
      if (!status.ok())
        return status;
      state.armed = true;
      state.longReported = false;
    } else {
      state.armed = true;
    }
  }

  if (state.stablePressed && state.armed && !state.longReported &&
      hasElapsed(nowMs, state.pressedAtMs, config_.longPressMs)) {
    const Status status = enqueue(index, ButtonEventType::LONG_PRESS, nowMs);
    if (!status.ok())
      return status;
    state.longReported = true;
  }
  return Status();
}

} // namespace ca
