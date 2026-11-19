#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"

// ESP32 cart entry point. DevKit V1 is the initial reference; alternate ESP32 boards
// need their own pin profile.
void setup() {
  // TODO: after board selection, construct ESP32 adapters and CartApplication.
}
void loop() {
  // TODO: service CartApplication non-blockingly; continue receiving while
  // NOT_ACCEPTING.
}
