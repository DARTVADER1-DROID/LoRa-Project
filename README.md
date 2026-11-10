# LoRa Project

This repository contains two Arduino/LoRa firmware prototypes and a cross-platform desktop application for the messaging project.

## Applications

### LoRa messaging firmware

`firmware/messaging/` contains the shared-group messaging firmware with peer discovery, message identifiers, acknowledgements, bounded retries, and delivery outcome reporting.

### LoRa Messaging desktop app

`messaging-app/` is a Rust + Tauri 2 desktop application with a plain HTML, CSS, and JavaScript frontend. On the current `main` branch it:

- Discovers operating-system serial ports at startup.
- Refreshes the port list periodically and on demand.
- Displays port names, transport type, and USB metadata when available.

Port discovery does not prove that a device is an Arduino or that its firmware or LoRa radio is healthy. Explicit connection management and firmware handshake work are tracked separately on the `feature/serial-connect-status` branch.

### Campus assistance firmware

`firmware/campus_assistance/` is a separate campus mobility-assistance prototype using fixed pickup stations and electric carts.

## Repository layout

- `firmware/messaging/` — LoRa group-messaging firmware
- `firmware/campus_assistance/` — campus assistance firmware
- `messaging-app/` — Rust + Tauri desktop application
- `docs/` — hardware, protocol, and reliability design documents
- `tests/` — host-side protocol and reliability tests

## Hardware and safety

The initial messaging hardware target is an Arduino Uno with an SX1278-based LoRa module. Use a properly regulated 3.3 V supply and suitable 5 V-to-3.3 V logic-level shifting.

**RF compatibility and regulatory warning:** SX1278 modules commonly operate around 433 MHz and are not interchangeable with 865–867 MHz or 868/915 MHz modules. Verify the exact radio, antenna, local WPC requirements, permitted power, bandwidth, and duty-cycle before transmitting.

Read [hardware and safety notes](docs/HARDWARE_AND_SAFETY.md) before wiring or powering hardware.

## Build and test

- Desktop app: use Cargo from `messaging-app/`; see [desktop app instructions](messaging-app/README.md).
- Firmware: compile the sketches with Arduino CLI or Arduino IDE.
- Host-side tests: `python3 -m unittest discover -s tests -v`.

GitHub Actions compiles the Arduino firmware variants and runs the source-contract tests.

## Development status

This is incremental prototype development. Neither firmware nor the desktop application is safety-certified or ready for unattended deployment. Compilation and host-side tests do not replace physical radio, electrical, hardware-in-the-loop, and field validation.
