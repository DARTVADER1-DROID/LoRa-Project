# Campus Assistance — LoRa-Based Campus Mobility Support

This repository is now focused on the **Campus Assistance System for blind and visually impaired students**.

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

Start with [the Campus Assistance documentation index](docs/campus-assistance/README.md), then follow its ten engineering documents and supporting diagrams.

## Repository structure

- `docs/campus-assistance/` — active project specification and engineering documentation.
- `archive/` — preserved legacy firmware, desktop application, tests, old documentation and CI workflow. These files are historical reference only and are not part of the active implementation baseline.
- `.gitignore` — repository-wide local build-artifact exclusions.

## Firmware development status

**The active firmware implementation has intentionally been cleared.** The next implementation work will begin with a detailed Low-Level Design (LLD), based on the approved project requirements. No archived firmware should be treated as compliant with the current target design.

## Safety and regulatory gates

The system is an assistance-request and dispatch communication system, not an autonomous navigation or vehicle-control system. It must not control steering, braking, throttle or traction systems.

The RA-02/SX1278 is commonly a 433 MHz module and cannot be software-tuned to another band. Verify the exact module, antenna, frequency, EIRP, bandwidth, duty cycle and applicable Indian WPC requirements before any real-world transmission.

No RF range, reliability, battery-life, accessibility or safety outcome is claimed without measurements and validation.
