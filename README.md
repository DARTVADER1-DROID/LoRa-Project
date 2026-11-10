# LoRa Project

This repository contains two Arduino firmware applications and a cross-platform desktop application for communicating with LoRa devices.

## Applications

### 1. LoRa messaging firmware — `firmware/messaging/`

A broadcast group-messaging prototype for short text messages. It includes peer discovery, message acknowledgements, bounded retries, and Serial Monitor input/output.

### 2. Campus assistance firmware — `firmware/campus_assistance/`

A campus mobility-assistance prototype built around fixed pickup stations and electric carts. It includes broadcast requests, FIFO queues, claim-and-confirm assignment, cancellation, and offline recovery.

### 3. LoRa messaging desktop app — `messaging-app/`

A cross-platform desktop application built with **Rust + Tauri 2**. Its frontend uses plain HTML, CSS, and JavaScript.

**Current desktop-app milestone: serial-port discovery.**

- Scans for operating-system serial ports when the application starts.
- Refreshes the list automatically every two seconds.
- Provides a manual refresh action.
- Displays the port name, connection type, manufacturer, device product, and USB VID/PID when available.
- Does not open a serial port or communicate with the Arduino yet.

Finding a serial port only means the operating system exposes it. It does not prove that the connected board is running the expected firmware, that its LoRa radio initialized, or that another LoRa device is reachable.

See [the desktop app README](messaging-app/README.md) for prerequisites, commands, architecture, and current limitations.

## Initial hardware target

- Arduino Uno (ATmega328P)
- SX1278-based LoRa module (for example, a correctly identified Ra-02 variant)
- 3.3 V regulated supply for the radio
- Proper 5 V-to-3.3 V logic level shifting on Uno-to-radio signals

**RF compatibility warning:** SX1278 silicon is a sub-GHz part commonly used in 433 MHz modules; it is not interchangeable with 868/915 MHz module variants. Confirm the exact module's RF frequency, antenna, local WPC authorization/exemption, power, bandwidth, and duty-cycle requirements before transmitting. Firmware frequency must match the physical module.

## Development status

The repository is being developed incrementally. The desktop application currently implements only serial-port discovery; serial connection management, firmware identification, desktop-to-device messaging, and integration with the LoRa messaging protocol are not implemented yet.

The firmware and desktop application are prototypes. They have not been established as safety-certified or ready for unattended campus deployment. A real deployment needs field RF testing, accessibility testing with intended users, electrical and vehicle safety review, and operational procedures.

Do not interpret documentation of a planned feature as evidence that it is implemented or tested.

## Repository layout

- `firmware/messaging/` — LoRa group-messaging firmware
- `firmware/campus_assistance/` — campus assistance firmware
- `messaging-app/` — Rust + Tauri desktop application
- `docs/` — firmware protocol, reliability, and hardware notes
- `tests/` — host-side protocol tests

See [`docs/HARDWARE_AND_SAFETY.md`](docs/HARDWARE_AND_SAFETY.md) before wiring or powering hardware.
