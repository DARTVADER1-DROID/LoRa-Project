# LoRa Messaging Desktop App

A cross-platform desktop application for connecting a computer to an Arduino-based LoRa device over USB serial.

## Technology

- **Backend:** Rust
- **Desktop framework:** Tauri 2
- **Frontend:** plain HTML, CSS, and JavaScript
- **Serial-port enumeration:** Rust `serialport` crate
- **Frontend toolchain:** none; no Node.js, npm, or frontend framework is required

The application uses the operating system's web view through Tauri. It does not use Qt.

## Current implementation

<badge>Implemented</badge> is represented below as plain status labels in this document; the status refers to code present in the repository, not a verified hardware test.

### Serial-port discovery

1. The application enumerates serial ports when it starts.
2. It refreshes the list automatically every two seconds.
3. The user can trigger a scan with the **Refresh** button.
4. The interface displays:
   - Port name (for example, a platform-specific COM or device path)
   - Connection type (USB, PCI, Bluetooth, or unknown)
   - USB manufacturer and product strings, when supplied by the operating system
   - USB vendor ID and product ID, when available
5. An empty state is shown when no serial ports are detected, and an error is displayed if enumeration fails.

The app does not automatically open detected ports.

### What detection does and does not mean

Serial-port enumeration tells the application that the operating system currently exposes a serial interface. It does **not** confirm any of the following:

- That the port belongs to the expected Arduino.
- That the application can successfully open the port.
- That the expected firmware is running.
- That the SX1278/LoRa radio initialized successfully.
- That another LoRa device is powered on or reachable.

These are separate connection and health checks and have not been implemented in the desktop app yet.

## Project structure

```text
messaging-app/
├── frontend/
│   ├── index.html       # Application window markup
│   ├── app.js           # Port-list UI and refresh behavior
│   └── styles.css       # Application styling
├── src-tauri/
│   ├── Cargo.toml       # Rust and Tauri dependencies
│   ├── build.rs         # Tauri build integration
│   ├── tauri.conf.json  # Window and frontend configuration
│   └── src/
│       └── main.rs      # Tauri entry point and serial enumeration command
└── README.md
```

## Prerequisites

Install the stable Rust toolchain and the platform-specific Tauri prerequisites.

- **Windows:** Microsoft C++ Build Tools with the MSVC toolchain, and the WebView2 Runtime.
- **Linux:** the GTK and WebKitGTK development packages required by Tauri for your distribution.
- **macOS:** Xcode Command Line Tools.

No Qt, Node.js, npm, or separate frontend build step is required by this project.

## Run

Open a terminal in the `messaging-app/` directory:

```sh
cargo run --manifest-path src-tauri/Cargo.toml
```

The first build downloads the Rust dependencies and compiles the native application. Subsequent runs use the local Cargo build cache.

## Build a release executable

From the `messaging-app/` directory:

```sh
cargo build --release --manifest-path src-tauri/Cargo.toml
```

The release executable is written under:

```text
src-tauri/target/release/
```

The current Tauri configuration has bundling disabled, so this command builds the executable; it does not create a platform installer.

## Current limitations

- No serial port is opened or held by the application.
- There are no connect or disconnect controls.
- There is no Arduino identity or firmware handshake.
- There is no heartbeat or radio-readiness check.
- There is no desktop serial message reader or writer.
- The desktop app is not yet integrated with the firmware's LoRa messaging protocol.
- Build and hardware behavior have not yet been verified across Windows, Linux, and macOS.

## Incremental development plan

The next steps, in dependency order, are:

1. Add user-controlled serial connect and disconnect.
2. Define and implement a desktop-to-firmware handshake so the app can identify the expected device and determine firmware/radio readiness.
3. Implement buffered serial input and output.
4. Connect the desktop messaging interface to the existing firmware messaging protocol.

Each step should be implemented and verified before moving to the next. The desktop-to-Arduino serial protocol must be designed explicitly; it is separate from the over-the-air LoRa packet protocol.
