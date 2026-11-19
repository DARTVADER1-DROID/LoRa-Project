#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"

// ESP32 cart entry point. Exact board and pin map are TBD.
void setup() {
  // TODO: after board selection, construct ESP32 adapters and CartApplication.
}
void loop() {
  // TODO: service CartApplication non-blockingly; continue receiving while
  // NOT_ACCEPTING.
}
