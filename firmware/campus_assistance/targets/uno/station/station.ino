#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"
#include "ca/platform/uno/uno_r3_profile.hpp"

using namespace ca;
using namespace ca::platform;

const BoardPinMap kBoardPins = ArduinoUnoR3Profile::pins();
ArduinoSx1278Transport radio(kBoardPins, SPI);
ArduinoButtonPinReader buttonReader(kBoardPins);
DebouncedButtonInput buttons(buttonReader);
ArduinoPcf8574Lcd display(kBoardPins, 0x27);
ArduinoBuzzer buzzer(kBoardPins.buzzerPin);

// Hardware composition is not yet wired to the role application.
void setup() {
  (void)radio;
  (void)buttons;
  (void)display;
  (void)buzzer;
}

void loop() {
  // The eventual StationApplication service call must remain non-blocking.
}
