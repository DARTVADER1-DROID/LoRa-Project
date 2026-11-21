# RA-02 Serial Chat Test

A deliberately standalone point-to-point text test for **only an MCU and an AI-Thinker RA-02 (SX1278)**. It does not include or depend on the Campus Assistance application, LCDs, buttons, buzzers, EEPROM, or other peripherals.

## One common radio profile

Every sketch uses exactly the same LoRa modem configuration. Do not mix sketches built with different settings.

| Parameter | Value |
|---|---:|
| RF frequency | **433.000 MHz** |
| Signal bandwidth | **125 kHz** |
| Spreading factor | **SF9** |
| Coding rate | **4/5** |
| Preamble length | **8 symbols** |
| Sync word | **0x12** |
| CRC | **Enabled** |
| TX power | **10 dBm** |
| Serial baud | **115200** |
| Maximum text line | **200 bytes** |

SF9 is selected as a more robust initial bench profile than SF7, with a lower data rate. All communicating nodes must use the same values.

The 433 MHz setting is for the RA-02/SX1278 hardware. Verify current Indian WPC requirements for the exact module, antenna, EIRP and duty cycle before transmitting over the air.

## Supported boards and wiring

All ESP32 profiles below use the same explicitly assigned GPIO map. The sketches are separate so each can be opened and uploaded independently in Arduino IDE.

### Arduino Uno R3

| RA-02 pin | Uno R3 pin | Notes |
|---|---:|---|
| VCC | External regulated 3.3 V | Do not power from Uno 3.3 V pin; regulator must handle TX current peaks |
| GND | GND | Common ground |
| SCK | D13 | Through 5 V-to-3.3 V level shifter |
| MISO | D12 | RA-02 3.3 V output |
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
| VCC | 3V3 (use an external regulated 3.3 V supply if the board rail is inadequate) |
| GND | GND |
| SCK | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| NSS / CS | GPIO17 (often labelled TX2 / IO17) |
| RESET | GPIO14 |
| DIO0 | GPIO26 |
| ANT | 433 MHz antenna |

In Arduino IDE select **DOIT ESP32 DEVKIT V1** or the matching DevKit V1 board definition. GPIO18/19/23 are explicitly configured SPI pins.

## Arduino IDE: compile and upload

1. Install the **LoRa** library by Sandeep Mistry (Library Manager: search for LoRa).
2. Open exactly one sketch:
   - Uno: **src/uno/uno.ino**
   - WROOM-32: **src/wroom32/wroom32.ino**
   - DevKit V1: **src/esp32devkitv1/esp32devkitv1.ino**
3. Select the matching board and COM port.
4. Verify and upload.
5. Open Serial Monitor at **115200 baud** and set line ending to **Newline**. No Line Ending will not trigger the firmware's send operation.

## PlatformIO: build and upload

Open this directory in PlatformIO:

    pio run -e uno
    pio run -e wroom32
    pio run -e esp32devkitv1

Upload using the matching environment:

    pio run -e uno -t upload
    pio run -e wroom32 -t upload
    pio run -e esp32devkitv1 -t upload

The original **esp32dev** environment remains as a backward-compatible alias and uses the same radio profile.

## What the serial diagnostics mean

At startup, each sketch prints **RADIO READY** and the complete common radio profile. This confirms the radio initialization call succeeded and the settings were applied.

When you type a line and press Enter:
- **TX OK** means the library completed the local transmit operation and queued the expected number of bytes. It does **not** prove that another radio received the packet.
- **TX FAILED** means the local transmit operation did not complete successfully.
- **RX** packet diagnostics show reported payload length, RSSI, SNR, readable payload bytes, and actual bytes read. Non-printable bytes are shown in hexadecimal.

The sketches deliberately do not print empty receive lines when no packet is detected. They also do not treat a device's own transmission as a received message.

## Important: testing with only one RA-02

With one RA-02, you can validate:
1. The ESP32/Uno starts and prints **RADIO READY**.
2. The configured profile is printed correctly.
3. Typing a message with **Newline** produces **TX OK**.

You **cannot validate over-the-air reception or a complete send/receive exchange with only one module**. The RA-02 is a half-duplex transceiver; it switches to transmit mode and does not provide a self-loopback of its own transmitted packet. A second compatible RA-02 + MCU is required for an actual receive test. Do not interpret the absence of RX on a one-module setup as a fault.

## Two-node messaging test

1. Upload the same profile to two MCU + RA-02 nodes.
2. Attach suitable 433 MHz antennas to both modules.
3. Power both from stable regulated 3.3 V supplies and connect common grounds within each node.
4. Open both Serial Monitors at 115200 baud with **Newline** selected.
5. Wait for **RADIO READY** on both.
6. Type HELLO FROM NODE A on node A. Node A should print **TX OK**; node B should print an RX packet with the exact text.
7. Reply with HELLO FROM NODE B from node B and verify node A receives it.

This is a raw text demo, not the production Campus Assistance packet protocol; it has no addressing, acknowledgements, retries, encryption or collision avoidance.

## Electrical and RF precautions

- RA-02 is a 3.3 V-only module. Never connect it to 5 V supply or logic.
- Provide a regulated 3.3 V rail with adequate transient-current capability and local decoupling; the radio can draw substantial current during TX.
- ESP32 GPIO is 3.3 V logic. Do not apply 5 V to ESP32 pins.
- Connect a suitable 433 MHz antenna before transmitting.
- For Uno, level-shift SCK, MOSI, NSS and RESET from 5 V to 3.3 V.
- Keep SPI wires short and connect all grounds.

## Troubleshooting

- **RADIO READY missing:** check 3.3 V at the module, common GND, CS/NSS, RESET, SPI pins and level shifting.
- **TX FAILED:** check power stability, SPI wiring and module connection.
- **TX OK but no RX on a two-node test:** verify both nodes use the identical profile; check both antennas, DIO0 wiring, power rails and distance.
- **Random resets or corrupted packets:** improve the 3.3 V regulator/decoupling and shorten wiring.
