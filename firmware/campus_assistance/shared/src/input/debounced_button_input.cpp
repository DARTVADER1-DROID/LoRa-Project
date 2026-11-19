#include "ca/input/debounced_button_input.hpp"

namespace ca {

DebouncedButtonInput::DebouncedButtonInput(IButtonPinReader& reader)
    : reader_(reader), debouncer_(), config_(), lastStatus_(), initialized_(false) {
}

Status DebouncedButtonInput::begin(const ButtonConfig& config) {
  if (initialized_)
    return Status(ErrorCode::INVALID_STATE);
  const Status readerStatus = reader_.begin(config);
  if (!readerStatus.ok())
    return readerStatus;
  const Status debounceStatus = debouncer_.configure(config);
  if (!debounceStatus.ok())
    return debounceStatus;
  config_ = config;
  lastStatus_ = Status();
  initialized_ = true;
  return Status();
}

void DebouncedButtonInput::service(uint32_t nowMs) {
  if (!initialized_) {
    lastStatus_ = Status(ErrorCode::INVALID_STATE);
    return;
  }

  bool pressed[kMaxPhysicalButtons] = {false, false};
  for (uint8_t i = 0; i < config_.buttonCount; ++i)
    pressed[i] = reader_.isPressed(i);
  lastStatus_ = debouncer_.update(pressed, config_.buttonCount, nowMs);
}

bool DebouncedButtonInput::pollEvent(ButtonEvent& event) {
  return initialized_ && debouncer_.pollEvent(event);
}

Status DebouncedButtonInput::lastStatus() const {
  return lastStatus_;
}

} // namespace ca
