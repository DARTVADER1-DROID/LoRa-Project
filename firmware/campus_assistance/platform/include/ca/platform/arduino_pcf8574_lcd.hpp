#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <Wire.h>

#include "ca/platform/board_profile.hpp"
#include "ca/ports/hardware.hpp"

namespace ca {
namespace platform {

// PCF8574 backpack mapping used by the common LCD1602 modules:
// P0=RS, P1=RW, P2=EN, P3=backlight, P4..P7=D4..D7.
// The mapping and I2C address are explicit constructor inputs because clone
// backpacks vary. This adapter never allocates memory.
class ArduinoPcf8574Lcd final : public IDisplay {
public:
  ArduinoPcf8574Lcd(const BoardPinMap& pins,
                    uint8_t address,
                    uint8_t rsBit = 0,
                    uint8_t rwBit = 1,
                    uint8_t enableBit = 2,
                    uint8_t backlightBit = 3)
      : pins_(pins), address_(address), rsBit_(rsBit), rwBit_(rwBit),
        enableBit_(enableBit), backlightBit_(backlightBit),
        backlightMask_(static_cast<uint8_t>(1U << backlightBit)), initialized_(false),
        backlightOn_(true) {
    for (uint8_t row = 0; row < 2; ++row)
      for (uint8_t column = 0; column < 16; ++column)
        previous_[row][column] = '\0';
  }

  Status begin() override {
    if (initialized_ || !isValidPinMap(pins_) || address_ < 0x08 || address_ > 0x77 ||
        rsBit_ > 7 || rwBit_ > 7 || enableBit_ > 7 || backlightBit_ > 7 ||
        rsBit_ == rwBit_ || rsBit_ == enableBit_ || rsBit_ == backlightBit_ ||
        rwBit_ == enableBit_ || rwBit_ == backlightBit_ || enableBit_ == backlightBit_)
      return Status(ErrorCode::CONFIGURATION_INVALID);

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    Wire.begin(static_cast<int>(pins_.displaySda), static_cast<int>(pins_.displayScl));
#else
    Wire.begin();
#endif
    backlightMask_ = static_cast<uint8_t>(1U << backlightBit_);
    delay(50);
    writeExpander(backlightMask_);
    writeNibble(0x30, false);
    delay(5);
    writeNibble(0x30, false);
    delayMicroseconds(150);
    writeNibble(0x30, false);
    writeNibble(0x20, false);
    command(0x28); // 4-bit, two-line, 5x8 font.
    command(0x08); // Display off while clearing.
    command(0x01);
    delay(2);
    command(0x06); // Increment cursor, no display shift.
    command(0x0C); // Display on, cursor and blink off.
    initialized_ = true;
    return Status();
  }

  void render(const UiViewModel& view) override {
    if (!initialized_)
      return;

    renderRow(0, view.line1);
    renderRow(1, view.line2);
  }

  void setBacklight(bool enabled) {
    if (backlightBit_ > 7)
      return;
    backlightMask_ = enabled ? static_cast<uint8_t>(1U << backlightBit_) : 0;
    if (initialized_)
      writeExpander(backlightMask_);
  }

private:
  BoardPinMap pins_;
  uint8_t address_;
  uint8_t rsBit_;
  uint8_t rwBit_;
  uint8_t enableBit_;
  uint8_t backlightBit_;
  uint8_t backlightMask_;
  bool initialized_;
  char previous_[2][16];

  void writeExpander(uint8_t value) {
    Wire.beginTransmission(address_);
    Wire.write(value);
    Wire.endTransmission();
  }

  void writeNibble(uint8_t value, bool data) {
    const uint8_t high = static_cast<uint8_t>(
        (value & 0xF0U) | (data ? (1U << rsBit_) : 0U) | backlightMask_);
    Wire.beginTransmission(address_);
    Wire.write(static_cast<uint8_t>(high | (1U << enableBit_)));
    Wire.write(high);
    Wire.endTransmission();
  }

  void send(uint8_t value, bool data) {
    const uint8_t mode = data ? static_cast<uint8_t>(1U << rsBit_) : 0U;
    const uint8_t high = static_cast<uint8_t>((value & 0xF0U) | mode | backlightMask_);
    const uint8_t low =
        static_cast<uint8_t>(((value << 4) & 0xF0U) | mode | backlightMask_);
    Wire.beginTransmission(address_);
    Wire.write(static_cast<uint8_t>(high | (1U << enableBit_)));
    Wire.write(high);
    Wire.write(static_cast<uint8_t>(low | (1U << enableBit_)));
    Wire.write(low);
    Wire.endTransmission();
  }

  void command(uint8_t value) { send(value, false); }

  void renderRow(uint8_t row, const char* text) {
    bool changed = false;
    char next[16];
    for (uint8_t column = 0; column < 16; ++column) {
      const char value = text != 0 && text[column] != '\0' ? text[column] : ' ';
      next[column] = value;
      changed = changed || previous_[row][column] != value;
    }
    if (!changed)
      return;

    command(row == 0 ? 0x80 : 0xC0);
    for (uint8_t column = 0; column < 16; ++column) {
      send(static_cast<uint8_t>(next[column]), true);
      previous_[row][column] = next[column];
    }
  }
};

} // namespace platform
} // namespace ca

#endif // ARDUINO
