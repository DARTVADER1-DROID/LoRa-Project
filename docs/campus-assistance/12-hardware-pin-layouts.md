# Hardware Pin Layouts

Reference pin layouts for the currently documented Arduino Uno R3 and ESP32-WROOM-32 DevKit V1. The ESP32 layout is specific to that DevKit profile and is not universal across ESP32 boards.

## RA-02 / SX1278 connections

| Signal | Arduino Uno R3 | ESP32-WROOM-32 DevKit V1 |
|---|---:|---:|
| SCK | D13 | GPIO18 |
| MISO | D12 | GPIO19 |
| MOSI | D11 | GPIO23 |
| NSS / CS | D10 | GPIO17 |
| RESET | D9 | GPIO14 |
| DIO0 | D2 | GPIO26 |
| Supply | Regulated 3.3 V external supply | 3.3 V regulated supply |
| Ground | GND | GND |

### Electrical requirements

- RA-02 is a 3.3 V radio module. Never connect it to 5 V supply or 5 V logic.
- Uno SPI/control outputs (SCK, MOSI, NSS, RESET) require 5 V-to-3.3 V level translation. MISO and DIO0 are 3.3 V outputs.
- Provide a regulated 3.3 V rail with adequate transient current capability and local decoupling. Do not assume the Uno onboard 3.3 V output can power the RA-02 reliably.
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
