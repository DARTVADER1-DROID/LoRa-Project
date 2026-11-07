# LoRa Project

Two independent Arduino firmware applications built around LoRa radios:

1. `firmware/messaging/` — one common broadcast group for short text messages, dynamic peer discovery, automatic acknowledgements, bounded retries, and Serial Monitor input/output.
2. `firmware/campus_assistance/` — fixed pickup stations and electric carts for campus mobility assistance, with broadcast requests, FIFO queues, claim-and-confirm assignment, cancellation, and offline recovery.

## Initial hardware target

- Arduino Uno (ATmega328P)
- SX1278-based LoRa module (for example, a correctly identified Ra-02 variant)
- 3.3 V regulated supply for the radio
- Proper 5 V-to-3.3 V logic level shifting on Uno-to-radio signals

**RF compatibility warning:** SX1278 silicon is a sub-GHz part commonly used in 433 MHz modules; it is not interchangeable with 868/915 MHz module variants. Confirm the exact module's RF frequency, antenna, local WPC authorization/exemption, power, bandwidth, and duty-cycle requirements before transmitting. Firmware frequency must match the physical module.

## Development status

This repository is being built incrementally. The initial milestone is a compileable prototype and host-side protocol tests. It is not safety-certified or ready for unattended campus deployment. A real deployment needs field RF testing, accessibility testing with intended users, electrical and vehicle safety review, and operational procedures.

## Repository layout

- `firmware/messaging/` — messaging application
- `firmware/campus_assistance/` — campus assistance application
- `docs/` — protocol, wiring, and test notes
- `tests/` — host-side protocol tests

See `docs/HARDWARE_AND_SAFETY.md` before wiring or powering hardware.