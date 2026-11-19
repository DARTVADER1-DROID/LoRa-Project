#pragma once

#include "ca/platform/board_profile.hpp"

namespace ca {
namespace platform {

// Reference profile for the common ESP32-WROOM-32 DevKit V1.
// This is not a universal ESP32 pin map. Other boards must provide a new profile.
struct Esp32DevKitV1Profile {
  static const BoardPinMap pins() {
    const BoardPinMap value = {
        18, 19, 23, 17, 14, 26, 21, 22, {32, 33}, 2, 27};
    return value;
  }
};

} // namespace platform
} // namespace ca
