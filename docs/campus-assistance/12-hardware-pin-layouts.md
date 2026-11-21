# Hardware Pin Layouts

Reference pin layouts for Arduino Uno R3, ESP32-WROOM-32 modules on a suitable carrier, and ESP32 DevKit V1. ESP32 pin availability and board labels can vary; verify the exact board revision before wiring.

## RA-02 / SX1278 connections

| Signal | Arduino Uno R3 | ESP32-WROOM-32 | ESP32 DevKit V1 |
|---|---:|---:|---:|
| SCK | D13 | GPIO18 | GPIO18 |
| MISO | D12 | GPIO19 | GPIO19 |
| MOSI | D11 | GPIO23 | GPIO23 |
| NSS / CS | D10 | GPIO17 | GPIO17 |
| RESET | D9 | GPIO14 | GPIO14 |
| DIO0 | D2 | GPIO26 | GPIO26 |
| Supply | External regulated 3.3 V | Regulated 3.3 V | 3V3 (external regulated 3.3 V if required) |
| Ground | GND | GND | GND |

The ESP32 sketches explicitly configure SPI with SCK=18, MISO=19, MOSI=23 and CS=17. These are firmware-selected GPIO assignments, not a requirement that every ESP32 board use these pins.

## Board selection

- **ESP32-WROOM-32 bare module/carrier:** Arduino IDE board selection is generally **ESP32 Dev Module**. A bare module does not provide USB connectivity; the carrier must provide suitable power, boot/reset circuitry and USB-to-UART programming.
- **ESP32 DevKit V1:** select the matching **DOIT ESP32 DEVKIT V1** definition in Arduino IDE. This board commonly contains an ESP32-WROOM-32 module and has USB-to-UART onboard.

## Electrical requirements

- RA-02 is a 3.3 V radio module. Never connect it to 5 V supply or 5 V logic.
- Uno SPI/control outputs (SCK, MOSI, NSS, RESET) require 5 V-to-3.3 V level translation. MISO and DIO0 are 3.3 V outputs.
- Provide a regulated 3.3 V rail with adequate transient current capability and local decoupling. Do not assume the Uno onboard 3.3 V output can power the RA-02 reliably.
- ESP32 GPIO is 3.3 V logic; never apply 5 V to its GPIO pins.
- Connect a suitable 433 MHz antenna before transmitting.
- Verify applicable Indian WPC rules for the exact module, frequency, antenna, EIRP and duty cycle before RF operation.

## Other Campus Assistance I/O reference pins

These are the pin assignments from the current board profiles, not required for the standalone serial-chat test.

| Function | Arduino Uno R3 | ESP32 DevKit V1 |
|---|---:|---:|
| Display SDA | A4 | GPIO21 |
| Display SCL | A5 | GPIO22 |
| Button 1 | D4 | GPIO32 |
| Button 2 | D5 | GPIO33 |
| Buzzer | D6 | GPIO27 |

The standalone RA-02 serial-chat example uses only the MCU, RA-02, and USB serial connection. No display, button or buzzer is involved.
