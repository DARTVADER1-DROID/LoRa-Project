#include "ca/platform/esp32/devkit_v1_profile.hpp"
#include "ca/platform/uno/uno_r3_profile.hpp"

#include <assert.h>

int main() {
  const ca::platform::BoardPinMap esp32 = ca::platform::Esp32DevKitV1Profile::pins();
  const ca::platform::BoardPinMap uno = ca::platform::ArduinoUnoR3Profile::pins();
  assert(ca::platform::isValidPinMap(esp32));
  assert(ca::platform::isValidPinMap(uno));

  ca::platform::BoardPinMap conflict = esp32;
  conflict.buzzerPin = conflict.radioChipSelect;
  assert(!ca::platform::isValidPinMap(conflict));

  ca::platform::BoardPinMap missingButton = uno;
  missingButton.buttonPins[0] = -1;
  assert(!ca::platform::isValidPinMap(missingButton));

  ca::platform::BoardPinMap tooManyButtons = uno;
  tooManyButtons.buttonCount = 3;
  assert(!ca::platform::isValidPinMap(tooManyButtons));
  return 0;
}
