#pragma once

#include "ca/input/fixed_button_debouncer.hpp"
#include "ca/ports/hardware.hpp"

namespace ca {

// Minimal board-specific GPIO sampling seam. Implementations return logical
// pressed=true regardless of the electrical active level.
class IButtonPinReader {
public:
  virtual ~IButtonPinReader() {}
  virtual Status begin(const ButtonConfig& config) = 0;
  virtual bool isPressed(uint8_t buttonIndex) const = 0;
};

// Reusable IButtonInput adapter shared by every MCU target.
class DebouncedButtonInput final : public IButtonInput {
public:
  explicit DebouncedButtonInput(IButtonPinReader& reader);

  Status begin(const ButtonConfig& config) override;
  void service(uint32_t nowMs) override;
  bool pollEvent(ButtonEvent& event) override;
  Status lastStatus() const;

private:
  IButtonPinReader& reader_;
  FixedButtonDebouncer debouncer_;
  ButtonConfig config_;
  Status lastStatus_;
  bool initialized_;
};

} // namespace ca
