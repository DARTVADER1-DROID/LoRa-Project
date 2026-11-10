# LoRa Messaging Desktop App

Cross-platform desktop application built with **Rust + Tauri 2**. The frontend is plain HTML, CSS, and JavaScript; no Node.js frontend toolchain or frontend framework is required.

## Current functionality

- Discovers operating-system serial ports at startup.
- Refreshes the port list every two seconds and on demand.
- Lists port name, transport type, manufacturer, product, serial number, and USB VID/PID when supplied by the operating system.
- Lets the user select a detected port.
- Opens the selected port at **9600 baud**, matching the current messaging firmware's serial configuration.
- Provides explicit Connect and Disconnect controls; no port is opened automatically.
- Reports per-port availability, open/connected state, and serial-operation faults.
- Detects when an open port disappears from the operating-system port list and reports a disconnection fault.
- After opening a port, sends a bounded `@APP:IDENTIFY` request and recognizes the project messaging firmware by its `@DEVICE:` response.
- Displays the Arduino device ID, boot/session epoch, firmware version, protocol version, and reported LoRa initialization state.

## Status definitions and limitations

| Status | Meaning |
| --- | --- |
| Available | The operating system currently lists the serial port. |
| Port open | The application successfully opened the serial handle. |
| Fault | Opening the port failed, or a previously open port disappeared from the operating-system list. |
| Not connected | The port is listed but this app has not opened it. |

**Important:** A successful serial open alone does not identify the device. The identification handshake verifies that compatible project firmware responds and reports its startup LoRa initialization result. It does not prove RF communication with another radio, successful packet delivery, or network health. Devices with older firmware or generic USB-serial adapters remain unidentified rather than being mislabelled as Arduino devices. A port that remains listed but develops an I/O failure after opening may not be detected until serial I/O and protocol-level health checks are implemented.

The current firmware uses `Serial.begin(9600)`; the desktop connection currently uses this fixed baud rate.

## Technology

- **Backend:** Rust
- **Desktop shell:** Tauri 2
- **Frontend:** plain HTML, CSS, and JavaScript
- **Serial access:** Rust `serialport` crate
- **Frontend framework/build tool:** none

The frontend is served directly from `frontend/`; no Node.js, npm, React, Qt, or separate frontend build step is currently required. Tauri uses the platform's WebView implementation.

## Project structure

```text
messaging-app/
├── frontend/
│   ├── index.html       # App page structure and connection controls
│   ├── app.js           # Discovery, selection, connection UI, status refresh
│   └── styles.css       # Application styling
├── src-tauri/
│   ├── Cargo.toml       # Rust dependencies and package metadata
│   ├── build.rs         # Tauri build integration
│   ├── tauri.conf.json  # Tauri window, frontend, and security config
│   └── src/
│       └── main.rs      # Port enumeration and connection lifecycle commands
└── README.md
```

## Requirements

Install the Rust stable toolchain and the platform-specific Tauri prerequisites.

- **Windows:** Microsoft C++ Build Tools with the MSVC toolchain, and the WebView2 Runtime.
- **Linux:** the GTK and WebKitGTK development packages required by Tauri for your distribution.
- **macOS:** Xcode Command Line Tools.

No Qt installation, Node.js, npm, or frontend package manager is needed for the current implementation.

## Run locally

Open a terminal in this directory:

```sh
cargo run --manifest-path src-tauri/Cargo.toml
```

Cargo downloads the required Rust crates on the first build. Connect your Arduino or another USB serial device, select its port, and press **Connect**. Disconnect it using the **Disconnect** button.

## Compile an optimized executable

```sh
cargo build --release --manifest-path src-tauri/Cargo.toml
```

The executable is written under `src-tauri/target/release`:

- **Windows:** `lora-messaging-app.exe`
- **Linux/macOS:** `lora-messaging-app`

The current Tauri configuration has bundling disabled (`bundle.active: false`), so this command builds the native executable but does not create an installer or platform package. Build separately on each target operating system; this is not a single universal binary.

## Backend commands

- `list_serial_ports()` — enumerates ports and returns available USB metadata plus per-port availability, connection, and fault flags.
- `connect_serial_port(portName)` — opens the selected port at 9600 baud, requests device identity, and retains the serial handle plus any validated identity response.
- `disconnect_serial_port()` — closes the active serial handle.
- `get_connection_status()` — reports the current connection state and detects when the active port is no longer listed by the operating system.

Only one serial port can be connected at a time. Connecting a different port closes the previous handle before opening the new one.

## Next steps

1. Add buffered serial reads/writes and explicit framing.
2. Define and implement a versioned firmware handshake and heartbeat in both the desktop app and Arduino firmware.
3. Surface firmware and radio initialization diagnostics as separate states.
4. Integrate desktop messaging with the existing LoRa messaging protocol.

Serial debug output and application messages share the serial byte stream, so framing and message separation must be designed explicitly before exchanging application data.

## Validation status

The source changes are committed to the feature branch. A successful Rust/Tauri build, execution on each supported operating system, and tests with physical Arduino/LoRa hardware have **not yet been verified**.
