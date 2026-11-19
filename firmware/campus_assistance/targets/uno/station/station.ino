#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"

// Arduino Uno station entry point. Hardware composition is intentionally unimplemented.
void setup() {
  // TODO: construct Uno adapters and StationApplication; validate provisioned
  // station config.
}
void loop() {
  // TODO: service StationApplication using millis(); do not block radio/event
  // processing.
}
