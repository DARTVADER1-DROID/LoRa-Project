# Campus Assistance — LoRa-Based Campus Mobility Support

This repository is focused on the **Campus Assistance System for blind and visually impaired students**.

The active project specification is maintained in [docs/campus-assistance/](docs/campus-assistance/README.md). It defines the agreed target architecture, requirements, workflows, hardware constraints, LoRa protocol direction, firmware/data model, safety boundaries and verification plan.

## Current project baseline

- Fixed campus stations and electric carts communicate using LoRa only.
- Target MCU families: Arduino Uno and ESP32, with dedicated firmware builds and one common over-the-air protocol. The exact ESP32 board remains tentative.
- Retain Ai-Thinker RA-02 / SX1278, subject to verification of applicable Indian WPC requirements before field transmission.
- Every cart continuously listens and keeps a local FIFO list of unaccepted/unassigned requests.
- Driver explicitly enables persistent request acceptance; a cart can hold at most one active assignment.
- A station-authoritative claim/assignment handshake prevents multiple carts from owning the same request.
- Disabling acceptance clears that cart's local pending list only; it does not cancel the station-owned request.
- After journey completion, the driver must manually re-enable acceptance.
- Unaccepted requests expire after 10 minutes by default; the duration is firmware-configurable.
- Maximum two physical buttons per station/cart, with a 16×2 LCD and buzzer.

## Documentation

Start with [the Campus Assistance documentation index](docs/campus-assistance/README.md), then follow its engineering documents and supporting diagrams. The [Firmware LLD](docs/campus-assistance/11-firmware-lld/README.md) defines the implementation contracts.

## Repository structure

- `docs/campus-assistance/` — active project specification and engineering documentation.
- `firmware/campus_assistance/` — interface-only C++11 firmware contracts, target entry points, host compile smoke test and formatting tools.
- `.gitignore` — repository-wide local build-artifact exclusions.

## Firmware development status

The Firmware LLD has been translated into an interface-only C++ contract scaffold. It now defines explicit dependency-injected role composition, separate use cases, a single station assignment-commit authority, a complete request-record repository contract, byte-oriented radio transport and typed protocol events. Runtime implementations and hardware adapters remain intentionally pending.

A host CMake target and GitHub Actions checks are included for public-header compilation and formatting. These checks do not establish Arduino/ESP32 target buildability, radio functionality, memory/timing budgets or hardware correctness. No legacy firmware should be treated as compliant with the current target design.

## Safety and regulatory gates

The system is an assistance-request and dispatch communication system, not an autonomous navigation or vehicle-control system. It must not control steering, braking, throttle or traction systems.

The RA-02/SX1278 is commonly a 433 MHz module and cannot be software-tuned to another band. Verify the exact module, antenna, frequency, EIRP, bandwidth, duty cycle and applicable Indian WPC requirements before any real-world transmission.

No RF range, reliability, battery-life, accessibility or safety outcome is claimed without measurements and validation.
