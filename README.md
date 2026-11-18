# LoRa Project

This repository contains two Arduino/LoRa firmware prototypes and a cross-platform desktop application for the messaging project.

## Applications

### LoRa messaging firmware

`firmware/messaging/` contains the shared-group messaging firmware with peer discovery, message identifiers, acknowledgements, bounded retries, and delivery outcome reporting.

### LoRa Messaging desktop app

`messaging-app/` is a Rust + Tauri 2 desktop application with a plain HTML, CSS, and JavaScript frontend. It discovers and refreshes operating-system serial ports, displays available USB metadata, and provides explicit port selection, connect, and disconnect controls at 9600 baud. After connecting, it attempts a bounded `@APP:IDENTIFY` handshake and displays compatible firmware's device ID, boot/session epoch, firmware/protocol versions, and LoRa initialization status (READY or FAILED).

A serial-port open is not proof of firmware identity. A valid handshake confirms compatible firmware and reports its local LoRa initialization result; it does not prove RF communication with another node or end-to-end message delivery. See [the desktop app README](messaging-app/README.md) for details.

### Campus assistance firmware

`firmware/campus_assistance/` is a separate campus mobility-assistance prototype using fixed pickup stations and electric carts. One sketch is compiled in either station or cart role. The dedicated [Campus Assistance engineering documentation](docs/campus-assistance/README.md) describes the target system requirements, architecture, accessible workflow, hardware, LoRa protocol design, firmware model, safety and validation plan. It explicitly distinguishes the newly agreed target design from the current prototype implementation.

## Documentation

- [Detailed implementation report](docs/IMPLEMENTATION_REPORT.md) — architecture, hardware, pin mapping, firmware behavior, desktop app scope, safety boundaries, and remaining engineering work.
- [Application protocol](docs/PROTOCOL.md) — shared protocol principles and campus-assistance lifecycle.
- [Messaging reliability protocol](docs/MESSAGING_RELIABILITY_PROTOCOL.md) — normative RMP v1 delivery contract and implementation status.
- [Reliability design](docs/RELIABILITY_DESIGN.md) — system-level messaging and campus reliability requirements.
- [Verification and test plan](docs/VALIDATION_AND_TEST_PLAN.md) — compile matrix, functional tests, fault injection, RF measurements, and deployment gates.
- [Hardware and safety notes](docs/HARDWARE_AND_SAFETY.md) — electrical interface, RF compatibility, campus safety, and persistence limitations.
- [Campus Assistance — complete project documentation](docs/campus-assistance/README.md) — standalone engineering documentation suite for the blind/visually impaired student campus mobility assistance system.

## Repository layout

- `firmware/messaging/` — LoRa group-messaging firmware
- `firmware/campus_assistance/` — campus assistance firmware
- `messaging-app/` — Rust + Tauri desktop application
- `docs/` — hardware, protocol, implementation, and validation documents
- `tests/` — host-side protocol and reliability tests

## Hardware and safety

The initial messaging hardware target is an Arduino Uno with an SX1278-based LoRa module. Use a properly regulated 3.3 V supply and suitable 5 V-to-3.3 V logic-level shifting.

**RF compatibility and regulatory warning:** SX1278 modules commonly operate around 433 MHz and are not interchangeable with 865–867 MHz or 868/915 MHz modules. Verify the exact radio, antenna, local WPC requirements, permitted power, bandwidth, and duty-cycle before transmitting.

Read [hardware and safety notes](docs/HARDWARE_AND_SAFETY.md) before wiring or powering hardware.

## Build and test

- Desktop app: use Cargo from `messaging-app/`; see [desktop app instructions](messaging-app/README.md).
- Firmware: compile the sketches with Arduino CLI or Arduino IDE. Campus station and cart roles are separate builds; see the implementation report for the role configuration and pin map.
- Host-side tests: `python3 -m unittest discover -s tests -v`.

GitHub Actions compiles the Arduino firmware variants and runs the source-contract tests. Consult the [verification and test plan](docs/VALIDATION_AND_TEST_PLAN.md) for hardware-in-the-loop and RF validation that CI cannot perform.

## Development status

This is incremental prototype development. Neither firmware nor the desktop application is safety-certified or ready for unattended deployment. Compilation and host-side tests do not replace physical radio, electrical, hardware-in-the-loop, accessibility, and field validation. The implementation report distinguishes code that exists from work that remains to be validated or implemented.
