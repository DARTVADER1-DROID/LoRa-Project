# Platform adapter boundary

Implement one adapter set per selected board. Keep Arduino/ESP-IDF headers confined to platform code.

Required adapters: IRadioTransport (RA-02/SX1278), IButtonInput (maximum two buttons, debounce and short/long events), IDisplay (16×2 LCD), IBuzzer, IClock, IPersistence and IDiagnostics.

Do not freeze pin maps before exact ESP32 board selection. Uno-to-RA-02 logic-level translation and a suitable 3.3 V transient supply are hardware requirements. Verify exact radio band, antenna, EIRP and current Indian WPC requirements before transmission.


## Reference pin profiles

Pin maps are compile-time data, not part of shared domain/application code.

### ESP32-WROOM-32 DevKit V1 reference

| Function | GPIO |
|---|---:|
| RA-02 SPI SCK / MISO / MOSI | 18 / 19 / 23 |
| RA-02 NSS / RESET / DIO0 | 17 / 14 / 26 |
| 16×2 I2C LCD SDA / SCL | 21 / 22 |
| Buttons 1 / 2 | 32 / 33 |
| Buzzer | 27 |

The profile avoids using GPIO 6–11 (module flash pins), input-only pins for outputs,
and common boot-strapping pins for ordinary peripherals. This is a reference for
ESP32-WROOM-32 DevKit V1 only; board revisions and clone layouts must be checked.

### Arduino Uno R3 reference

| Function | Arduino pin |
|---|---:|
| RA-02 SPI SCK / MISO / MOSI | 13 / 12 / 11 |
| RA-02 NSS / RESET / DIO0 | 10 / 9 / 2 |
| 16×2 I2C LCD SDA / SCL | A4 / A5 |
| Buttons 1 / 2 | 4 / 5 |
| Buzzer | 6 |

Uno GPIO is 5 V while RA-02 is a 3.3 V device. Use a proper 3.3 V regulator with
transient current headroom and 5 V-to-3.3 V level translation on Uno outputs to
the radio. Never connect 5 V logic directly to RA-02 pins.

The profiles are validated for pin collisions by a host test. They do not prove
electrical compatibility, connector orientation, module breakout routing or
regulatory compliance. The actual Arduino/ESP32 radio, LCD, button, buzzer and
persistence adapters remain to be implemented.


## Implemented Arduino-core adapters

`platform/include/ca/platform/arduino_core_adapters.hpp` provides:
- `ArduinoButtonPinReader`: active-low INPUT_PULLUP GPIO sampling using an injected board pin map; gesture timing remains in the portable debouncer.
- `ArduinoClock`: rollover-compatible `millis()` clock.
- `ArduinoBuzzer`: non-blocking default-tone cue using `tone()`/`noTone()`.

These adapters are guarded by `ARDUINO` and remain outside the shared library. They
have not yet been compiled against a selected Arduino-ESP32 / AVR core version or
validated on physical boards.

`platform/include/ca/platform/arduino_pcf8574_lcd.hpp` provides a configurable
HD44780 16×2 LCD adapter over a PCF8574 I2C backpack. The I2C address and expander
bit mapping are constructor inputs because common backpack clones differ. It pads
short text rows and refreshes only when a row changes. The adapter still requires
compile and electrical verification with the actual backpack and Arduino core.


### Arduino SX127x transport reference

`platform/include/ca/platform/arduino_sx127x_transport.hpp` provides a poll-driven
raw-SPI SX1276/SX1278 LoRa-mode adapter using the injected board pin map. It supports
explicit-header LoRa frames with spreading factors 7–12, the enumerated bandwidth
set, coding rates 4/5–4/8 and PA_BOOST output 2–17 dBm. SF6 implicit-header mode and
20 dBm high-power operation are intentionally rejected by this adapter. TX completion
and RX events are polled without blocking the application loop; RX buffering is fixed
at two frames and overflow drops the frame without acknowledging it.

This is a reference driver, not yet validated against physical RA-02 modules, antenna
matching, DIO wiring, supply integrity, RF airtime or WPC rules. The 120-second TX
watchdog is a conservative guard for slow legal LoRa profiles and must be revisited
after airtime limits are frozen. Do not assume the configured frequency or power is
authorized for Indian operation.


## Arduino compile matrix

`platformio.ini` defines four target environments: Uno R3 station/cart and ESP32
DevKit V1 station/cart. Each environment compiles a small C++ wrapper that includes its actual role `.ino`
entry point, plus the shared portable sources, against its respective Arduino core.
This keeps the same sketch usable from Arduino IDE while ensuring PlatformIO links
its setup()/loop() entry points. The sketches
instantiate the selected board profile and hardware adapters, but the role
application composition roots are still placeholders. A successful PlatformIO
build therefore proves target/toolchain and adapter compatibility only; it does
not mean a complete application is ready to flash.


### Bounded persistent storage

arduino_byte_storage.hpp provides an AVR EEPROM byte store and an ESP32
Preferences/NVS byte store. The shared FixedDualSlotPersistence<SlotBytes>
alternates CRC-protected records and writes a commit marker last. The initial
reference allocation is 352 bytes of Uno EEPROM (two 176-byte slots) and a
768-byte ESP32 NVS blob (two 384-byte slots). These allocations are sized for
repository snapshots of four and ten records respectively; the complete Uno
SRAM/stack budget and hardware power-cut/endurance behavior still require
measurement and validation.
