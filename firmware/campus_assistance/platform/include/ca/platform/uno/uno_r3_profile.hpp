#pragma once

#include "ca/platform/board_profile.hpp"

namespace ca {
namespace platform {

// Reference profile for the classic 5 V Arduino Uno R3.
// RA-02 requires external 5 V-to-3.3 V logic translation on Uno SPI/control lines.
struct ArduinoUnoR3Profile {
  static const BoardPinMap pins() {
    const BoardPinMap value = {
        13, 12, 11, 10, 9, 2, 18, 19, {4, 5}, 2, 6};
    return value;
  }
};

} // namespace platform
} // namespace ca
