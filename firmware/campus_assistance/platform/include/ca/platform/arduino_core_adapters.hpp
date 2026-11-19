#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#include "ca/input/debounced_button_input.hpp"
#include "ca/platform/board_profile.hpp"
#include "ca/ports/hardware.hpp"
#include "ca/ports/runtime.hpp"

namespace ca {
namespace platform {

// Arduino-core GPIO sampler. The supplied pin profile determines the board;
// active-low INPUT_PULLUP wiring is used consistently across supported targets.
class ArduinoButtonPinReader final : public IButtonPinReader {
public:
  explicit ArduinoButtonPinReader(const BoardPinMap& pins)
      : pins_(pins), buttonCount_(0), initialized_(false) {}

  Status begin(const ButtonConfig& config) override {
    if (initialized_ || !isValidPinMap(pins_) ||
        config.buttonCount != pins_.buttonCount || config.buttonCount == 0 ||
        config.buttonCount > kMaxPhysicalButtons)
      return Status(ErrorCode::CONFIGURATION_INVALID);

    for (uint8_t i = 0; i < config.buttonCount; ++i)
      pinMode(pins_.buttonPins[i], INPUT_PULLUP);
    buttonCount_ = config.buttonCount;
    initialized_ = true;
    return Status();
  }

  bool isPressed(uint8_t buttonIndex) const override {
    if (!initialized_ || buttonIndex >= buttonCount_)
      return false;
    return digitalRead(pins_.buttonPins[buttonIndex]) == LOW;
  }

private:
  BoardPinMap pins_;
  uint8_t buttonCount_;
  bool initialized_;
};

class ArduinoClock final : public IClock {
public:
  uint32_t millisNow() const override { return millis(); }
};

class ArduinoSerialDiagnostics final : public IDiagnostics {
public:
  explicit ArduinoSerialDiagnostics(bool enabled = true, uint32_t baudRate = 115200UL)
      : enabled_(enabled), baudRate_(baudRate), initialized_(false) {}

  Status begin() {
    if (baudRate_ == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (enabled_) {
      Serial.begin(baudRate_);
      initialized_ = true;
    }
    return Status();
  }

  void record(uint16_t diagnosticCode, ErrorCode error) override {
    if (!enabled_ || !initialized_ || Serial.availableForWrite() < 24)
      return;
    Serial.print("CA:E:");
    Serial.print(diagnosticCode);
    Serial.print(':');
    Serial.println(static_cast<uint16_t>(error));
  }

  void increment(uint16_t diagnosticCode) override {
    if (!enabled_ || !initialized_ || Serial.availableForWrite() < 16)
      return;
    Serial.print("CA:C:");
    Serial.println(diagnosticCode);
  }

private:
  bool enabled_;
  uint32_t baudRate_;
  bool initialized_;
};

class ArduinoBuzzer final : public IBuzzer {
public:
  explicit ArduinoBuzzer(int8_t pin)
      : pin_(pin), config_(), active_(false), startedAtMs_(0) {}

  Status begin(const BuzzerConfig& config) override {
    if (pin_ < 0 || config.defaultToneHz == 0 || config.defaultDurationMs == 0)
      return Status(ErrorCode::CONFIGURATION_INVALID);
    config_ = config;
    pinMode(pin_, OUTPUT);
    noTone(pin_);
    active_ = false;
    return Status();
  }

  void playCue(uint8_t cueId) override {
    if (pin_ < 0 || cueId == 0)
      return;
    tone(pin_, config_.defaultToneHz);
    startedAtMs_ = millis();
    active_ = true;
  }

  void service(uint32_t nowMs) override {
    if (active_ && hasElapsed(nowMs, startedAtMs_, config_.defaultDurationMs)) {
      noTone(pin_);
      active_ = false;
    }
  }

private:
  int8_t pin_;
  BuzzerConfig config_;
  bool active_;
  uint32_t startedAtMs_;
};

} // namespace platform
} // namespace ca

#endif // ARDUINO
