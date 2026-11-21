# RA-02 Serial Chat Test

A deliberately standalone point-to-point text test for **only an MCU and an AI-Thinker RA-02 (SX1278)**. It does not include or depend on the Campus Assistance application, LCDs, buttons, buzzers, EEPROM, or other peripherals.

## Supported boards and wiring

All ESP32 profiles below use the same explicitly assigned GPIO map. The sketches are separate so each can be opened and uploaded independently in Arduino IDE.

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

### ESP32-WROOM-32 module

| RA-02 pin | ESP32 GPIO |
|---|---:|
| VCC | Regulated 3.3 V |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| NSS / CS | GPIO17 |
| RESET | GPIO14 |
| DIO0 | GPIO26 |
| ANT | 433 MHz antenna |

For a bare ESP32-WROOM-32 module, provide a suitable carrier circuit with stable 3.3 V power, EN/reset and USB-to-UART programming access. In Arduino IDE select **ESP32 Dev Module**. Do not apply 5 V to ESP32 GPIO or the RA-02.

### ESP32 DevKit V1

| RA-02 pin | ESP32 DevKit V1 pin |
|---|---:|
| VCC | 3V3 (external regulated 3.3 V recommended if board rail is inadequate) |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| NSS / CS | GPIO17 |
| RESET | GPIO14 |
| DIO0 | GPIO26 |
| ANT | 433 MHz antenna |

In Arduino IDE select **DOIT ESP32 DEVKIT V1** (or the matching DevKit V1 board definition installed in your ESP32 core). GPIO18/19/23 are the explicitly configured SPI pins; the sketch does not rely on the board's implicit SPI defaults.

## Arduino IDE: compile and upload

1. Install the **LoRa** library by Sandeep Mistry (Library Manager: search for LoRa).
2. Open exactly one sketch:
   - Uno: **src/uno/uno.ino**
   - WROOM-32: **src/wroom32/wroom32.ino**
   - DevKit V1: **src/esp32devkitv1/esp32devkitv1.ino**
3. Select the matching board and COM port.
4. Verify and upload.
5. Open Serial Monitor at **115200 baud**; set line ending to **Newline** or **Both NL & CR**.

## PlatformIO: build and upload

Install PlatformIO (VS Code extension or CLI), then open this directory:

    pio run -e uno
    pio run -e wroom32
    pio run -e esp32devkitv1
    pio run -e uno -t upload
    pio run -e wroom32 -t upload
    pio run -e esp32devkitv1 -t upload

The original **esp32dev** environment remains as a backward-compatible alias.

## Two-node messaging test

1. Build and upload the matching sketch to each MCU + RA-02 pair.
2. Use the same radio settings on both nodes.
3. Open both USB serial monitors at 115200 baud.
4. Wait for **Radio ready** on each board.
5. Type a line into either monitor and press Enter. The sender prints **TX:**; the other node prints **RX:** followed by the received text.
6. Reply from the other node to verify the reverse direction.

Maximum text length is 200 bytes per line. Oversized lines are discarded rather than sending a truncated message. This is a raw text demo, not the production Campus Assistance packet protocol; it has no addressing, acknowledgements, retries, encryption or collision avoidance.

## Shared radio settings

All sketches use 433.0 MHz, SF7, 125 kHz bandwidth, coding rate 4/5, sync word 0x12, and CRC enabled. Transmit power is set to 10 dBm as a conservative bench starting point. Verify current Indian WPC requirements for the exact module, frequency, antenna, EIRP and duty cycle before any over-the-air test. A 433 MHz module cannot be retuned to another band.

## Electrical and RF precautions

- RA-02 is a 3.3 V-only module. Never connect it to 5 V supply or logic.
- Provide a regulated 3.3 V rail with adequate transient current capability and local decoupling; the radio can draw substantial current during TX.
- ESP32 GPIO is 3.3 V logic. Do not apply 5 V to ESP32 pins.
- Connect a suitable 433 MHz antenna before transmitting.
- For Uno, level-shift SCK, MOSI, NSS and RESET from 5 V to 3.3 V. Do not use a resistor divider as a substitute for a suitable SPI level shifter.

## Troubleshooting

- **Radio ready** missing: check 3.3 V at the module during startup and transmission, common ground, CS/RESET wiring, SPI pins and level shifting.
- One-way communication: verify both boards use the same frequency and modem settings; check DIO0 wiring and antennas.
- Random resets or missing packets: improve the 3.3 V regulator/decoupling and shorten wiring.
