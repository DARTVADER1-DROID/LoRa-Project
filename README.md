# LoRa Project

A collection of Arduino/LoRa firmware applications and a cross-platform desktop application for the LoRa group-messaging project.

## Applications

### LoRa messaging firmware

`firmware/messaging/` contains Arduino firmware for a shared broadcast group with peer discovery, message identifiers, acknowledgements, bounded retries, and delivery outcome reporting. The desktop serial connection manager can identify compatible project firmware over USB serial and report its device identity and LoRa initialization result. Desktop messaging integration remains future work.

### LoRa Messaging desktop app

`messaging-app/` is a Rust + Tauri 2 desktop application with a plain HTML, CSS, and JavaScript frontend. Current desktop features:

- Automatic serial-port discovery at startup and periodic/manual refresh.
- Port listing with available USB device metadata.
- User-selected serial-port connection and explicit disconnect.
- Serial connection opened at 9600 baud, matching the current messaging firmware.
- Status reporting for available, open, disconnected, and serial-operation fault states.
- Detection of a connected device disappearing from the operating-system port list.
- Firmware identification handshake reporting Arduino device ID, boot/session epoch, firmware/protocol versions, and LoRa initialization status.

A successful serial open alone is not proof of firmware identity. The firmware handshake identifies compatible project firmware and reports LoRa initialization success/failure; it does not verify RF communication with another device or message delivery. Desktop message exchange remains future work. See [the desktop app README](messaging-app/README.md).

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

Development is incremental. The desktop app handles OS-level serial discovery, selected-port connection management, and identity handshakes with compatible messaging firmware. It does not yet exchange framed user messages with other radios or verify end-to-end RF delivery.

The firmware and protocol are prototypes, not safety-certified systems and not ready for unattended deployment. A real deployment requires hardware validation, field RF testing, electrical and operational safety review, and appropriate user testing.

Read [hardware and safety notes](docs/HARDWARE_AND_SAFETY.md) before wiring or powering hardware.
