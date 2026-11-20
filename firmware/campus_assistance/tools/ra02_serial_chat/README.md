# RA-02 Serial Chat Test

A deliberately standalone point-to-point text test for **only an MCU and an AI-Thinker RA-02 (SX1278)**. It does not include or depend on the Campus Assistance application, LCDs, buttons, buzzers, EEPROM, or other peripherals.

## Hardware and wiring

### Arduino Uno R3

| RA-02 pin | Uno R3 pin | Notes |
|---|---:|---|
| VCC | External regulated 3.3 V | Do not power from Uno 3.3 V pin; ensure regulator can handle TX current peaks |
| GND | GND | Common ground |
| SCK | D13 | Through 5 V-to-3.3 V level shifter |
| MISO | D12 | 3.3 V output is normally readable by Uno; use a suitable buffer if required by your design |
| MOSI | D11 | Through 5 V-to-3.3 V level shifter |
| NSS / CS | D10 | Through 5 V-to-3.3 V level shifter |
| RESET | D9 | Through 5 V-to-3.3 V level shifter |
| DIO0 | D2 | 3.3 V output |
| ANT | 433 MHz antenna | Attach a suitable antenna before transmitting |

**Never connect Uno 5 V logic directly to RA-02 inputs.** Use a proper 3.3 V regulator with adequate transient-current capability and level shifting on Uno-driven signals. Do not use a resistor divider as a substitute for a proper high-speed SPI level shifter.

### ESP32-WROOM-32 DevKit V1

| RA-02 pin | ESP32 GPIO |
|---|---:|
| VCC | 3V3 (verify supply capability; external regulated 3.3 V recommended) |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| NSS / CS | GPIO17 |
| RESET | GPIO14 |
| DIO0 | GPIO26 |
| ANT | 433 MHz antenna |

ESP32 GPIO is 3.3 V logic; do not apply 5 V to the radio or GPIO pins.

## Build and upload

Install PlatformIO (VS Code extension or CLI), then open this directory:

```sh
pio run -e uno
pio run -e esp32dev
pio run -e uno -t upload
pio run -e esp32dev -t upload
```

Connect the relevant board over USB and upload the matching environment. Open its serial monitor at **115200 baud** with line ending set to **Newline** (or Both NL & CR).

## Test

1. Wire and power both modules; attach antennas before RF transmission.
2. Upload `uno.ino` to the Uno and `esp32dev.ino` to the ESP32.
3. Open both serial monitors. Each must print `Radio ready`.
4. Type a line into either monitor and press Enter.
5. The sender prints `TX:` and the other board prints `RX:` followed by the same text.
6. Repeat in the opposite direction. Both boards listen continuously between transmissions.

Maximum text length is 200 bytes per line. This is a raw text demo, not the production Campus Assistance packet protocol; it has no addressing, acknowledgements, retries, encryption or collision avoidance.

## Shared radio settings

Both sketches use 433.0 MHz, SF7, 125 kHz bandwidth, coding rate 4/5, sync word 0x12, and CRC enabled. The transmit power is set to 10 dBm as a conservative bench starting point; confirm the exact module, antenna, EIRP, duty-cycle and current Indian WPC requirements before any over-the-air test. A 433 MHz module cannot be retuned to another band.

## Troubleshooting

- `Radio ready` missing: check 3.3 V at the module during startup and transmission, common ground, CS/RESET wiring, SPI pins and level shifting.
- One-way communication: verify both boards use the same frequency and modem settings; check DIO0 wiring and antennas.
- Random resets or missing packets: improve the 3.3 V regulator/decoupling and shorten wiring.
