# Hardware and safety notes

## Initial target

- Arduino Uno R3 / ATmega328P, 5 V logic
- SX1278-based radio module, commonly sold as a 433 MHz Ra-02
- LoRa Arduino library (Sandeep Mistry)
- Campus cart: 16x2 I2C LCD, three momentary buttons, buzzer and status LED
- Fixed station: one large request/cancel button, buzzer and status LED

## Critical RF compatibility

The SX1278 silicon and common Ra-02 boards are typically specified for approximately 137–525 MHz; many Ra-02 modules are 433 MHz. The 865–867 MHz Indian licence-exempt SRD allocation requires a radio/module variant that actually supports that band (commonly SX1276-based modules), not an SX1278/433 MHz module.

Do not set a frequency in firmware that the physical RF front-end cannot support. Before field operation, verify the exact module marking and datasheet, antenna frequency, Indian WPC rules, permitted EIRP, bandwidth, duty-cycle and channel requirements. This repository defaults to a clearly marked configurable value for bench development only; it is not a regulatory approval.

## Uno-to-radio electrical interface

The radio is a 3.3 V device. Never power a bare Ra-02 from the Uno 3.3 V pin unless the regulator's current capability and transient performance have been independently verified.

- Use a regulated 3.3 V supply with adequate transmit-current headroom and local decoupling.
- Shift Uno 5 V outputs down to 3.3 V for SCK, MOSI, NSS/CS and RESET. A suitable 3.3 V buffer/level shifter is preferred over resistive dividers for SPI.
- MISO is 3.3 V from the radio to the Uno; verify logic thresholds and wiring.
- Common ground is mandatory.
- Keep SPI wiring short and provide an antenna matched to the module's actual frequency.
- Do not transmit without a suitable antenna/load.

Typical SPI wiring: SCK=D13, MISO=D12, MOSI=D11, NSS=D10, RESET=D9, DIO0=D2. Adjust both wiring and `LoRa.setPins()` together.

## Campus safety

This is an assistance-request prototype, not a vehicle control system. It must never drive, steer, brake, or immobilize a cart. A driver remains responsible for safe vehicle operation. A lost radio link is not proof that a cart has stopped or that a student is safe. Provide an independent driver-visible warning and an operational fallback (radio/phone/help desk) before any real deployment.

The fixed station should clearly communicate whether a request is merely transmitted, received by a cart, accepted, cancelled, or expired. Audible patterns must be distinguishable and tested with visually impaired students and accessibility staff. Do not rely on LED colour alone.

## Memory and persistence

The Uno has 2 KB SRAM and 1 KB EEPROM. All queues and packet buffers must be bounded. EEPROM writes should be minimized and validated. Random values derived from floating analog pins are not cryptographic and do not guarantee globally unique IDs; commissioning must detect duplicate IDs. The messaging sketch stores a generated ID in EEPROM as a prototype convenience, not as a hardware serial number.
