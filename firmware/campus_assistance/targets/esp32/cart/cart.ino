#include <Arduino.h>

#include "CampusAssistance.h"
#include <SPI.h>
#include <Wire.h>
#include <Preferences.h>

#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"
#include "ca/platform/arduino_byte_storage.hpp"
#include "ca/storage/fixed_dual_slot_persistence.hpp"

#include "ca/platform/esp32/devkit_v1_profile.hpp"

using namespace ca;
using namespace ca::platform;

Esp32PreferencesByteStorage<768> byteStorage;
FixedDualSlotPersistence<384> persistence(byteStorage, 1);

const BoardPinMap kBoardPins = Esp32DevKitV1Profile::pins();
ArduinoSx1278Transport radio(kBoardPins, SPI);
ArduinoButtonPinReader buttonReader(kBoardPins);
DebouncedButtonInput buttons(buttonReader);
ArduinoPcf8574Lcd display(kBoardPins, 0x27);
ArduinoBuzzer buzzer(kBoardPins.buzzerPin);

// Hardware composition is not yet wired to the role application.
void setup() {
  const Status persistenceStatus = persistence.begin();
  (void)persistenceStatus;
  (void)radio;
  (void)buttons;
  (void)display;
  (void)buzzer;
}

void loop() {
  // Cart reception must remain active even while NOT_ACCEPTING.
}
