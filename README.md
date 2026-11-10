# LoRa Project

A collection of Arduino/LoRa firmware applications and a cross-platform desktop application for the LoRa group-messaging project.

## Applications

### LoRa messaging firmware

`firmware/messaging/` contains the Arduino firmware for a shared broadcast group. The current firmware includes peer discovery, message identifiers, acknowledgements, bounded retries, and delivery outcome reporting. Its current host interface is the Arduino serial interface; the desktop application has not yet been connected to this messaging protocol.

### LoRa Messaging desktop app

`messaging-app/` contains a Rust + Tauri 2 desktop application with a plain HTML, CSS, and JavaScript frontend. Its current implemented feature is serial-port discovery:

- Scans available operating-system serial ports at startup.
- Refreshes automatically every two seconds or on demand.
- Displays port name, transport type, USB manufacturer/product information, and USB VID/PID when available.
- Does not open a port or communicate with the Arduino yet.

See [the desktop app README](messaging-app/README.md) for prerequisites, commands, and current limitations.

### Campus assistance firmware

`firmware/campus_assistance/` contains a separate campus mobility-assistance prototype using fixed pickup stations and electric carts, with broadcast requests, FIFO queues, claim-and-confirm assignment, cancellation, and offline recovery.

## Hardware target

The initial messaging hardware target is:

- Arduino Uno (ATmega328P)
- SX1278-based LoRa module (for example, a correctly identified Ra-02 variant)
- Regulated 3.3 V supply for the radio
- Appropriate 5 V-to-3.3 V logic-level shifting between the Uno and radio

**RF compatibility and regulatory warning:** SX1278 is a sub-GHz silicon part commonly used in 433 MHz modules. It is not interchangeable with 868/915 MHz module variants. Confirm the exact module frequency, antenna, local WPC authorization/exemption, transmit power, bandwidth, and duty-cycle requirements before transmitting. Firmware frequency must match the physical module.

## Repository layout

- `firmware/messaging/` — Arduino group-messaging firmware
- `firmware/campus_assistance/` — campus assistance firmware
- `messaging-app/` — Rust + Tauri desktop application
- `docs/` — hardware, protocol, and reliability design documents
- `tests/` — host-side protocol and reliability tests

## Development status

Development is incremental. The desktop app is currently at the **serial-port discovery stage**; serial selection, connect/disconnect, firmware health checks, message exchange, and integration with the LoRa messaging protocol are not implemented yet.

The firmware and protocol are prototypes, not safety-certified systems and not ready for unattended deployment. A real deployment requires hardware validation, field RF testing, electrical and operational safety review, and appropriate user testing.

Read [hardware and safety notes](docs/HARDWARE_AND_SAFETY.md) before wiring or powering hardware.
