#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"

// Arduino Uno cart entry point. Receiver must run even while NOT_ACCEPTING.
void setup() {
  // TODO: construct Uno adapters and CartApplication; boot NOT_ACCEPTING and
  // reconcile state.
}
void loop() {
  // TODO: service CartApplication using millis(); never use delay() for
  // workflow timing.
}
