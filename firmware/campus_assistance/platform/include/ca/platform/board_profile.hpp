#pragma once

#include <stdint.h>

namespace ca {
namespace platform {

struct BoardPinMap {
  int8_t radioSck;
  int8_t radioMiso;
  int8_t radioMosi;
  int8_t radioChipSelect;
  int8_t radioReset;
  int8_t radioDio0;
  int8_t displaySda;
  int8_t displayScl;
  int8_t buttonPins[2];
  uint8_t buttonCount;
  int8_t buzzerPin;
};

inline bool isValidPinMap(const BoardPinMap& pins) {
  if (pins.radioSck < 0 || pins.radioMiso < 0 || pins.radioMosi < 0 ||
      pins.radioChipSelect < 0 || pins.radioReset < 0 || pins.radioDio0 < 0 ||
      pins.displaySda < 0 || pins.displayScl < 0 || pins.buzzerPin < 0 ||
      pins.buttonCount == 0 || pins.buttonCount > 2 ||
      pins.buttonPins[0] < 0 ||
      (pins.buttonCount > 1 && pins.buttonPins[1] < 0))
    return false;

  const int8_t activePins[11] = {
      pins.radioSck, pins.radioMiso, pins.radioMosi, pins.radioChipSelect,
      pins.radioReset, pins.radioDio0, pins.displaySda, pins.displayScl,
      pins.buttonPins[0], pins.buttonCount > 1 ? pins.buttonPins[1] : -1,
      pins.buzzerPin};
  for (uint8_t i = 0; i < 11; ++i) {
    if (activePins[i] < 0)
      continue;
    for (uint8_t j = 0; j < i; ++j)
      if (activePins[i] == activePins[j])
        return false;
  }
  return true;
}

} // namespace platform
} // namespace ca
