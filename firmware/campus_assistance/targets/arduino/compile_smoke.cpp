#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "ca/platform/arduino_core_adapters.hpp"
#include "ca/platform/esp32/devkit_v1_profile.hpp"
#include "ca/platform/uno/uno_r3_profile.hpp"
#include "ca/platform/arduino_pcf8574_lcd.hpp"
#include "ca/platform/arduino_sx127x_transport.hpp"

// PlatformIO compile smoke only. The Arduino IDE .ino files remain the role
// entry points; actual application composition is a separate implementation.
void setup() {
}

void loop() {
}
