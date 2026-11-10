# LoRa Messaging Desktop App

A cross-platform desktop application for communicating with the LoRa messaging firmware over a USB serial connection.

## Technology

- **Backend:** Rust
- **Desktop shell:** Tauri 2
- **Frontend:** plain HTML, CSS, and JavaScript
- **Serial-port enumeration:** Rust `serialport` crate
- **Frontend framework/build tool:** none

The frontend is served directly from `frontend/`; no Node.js, npm, React, Qt, or separate frontend build step is currently required. Tauri uses the platform's WebView implementation.

## Current implementation

The app currently implements **serial-port discovery only**.

- Enumerates serial ports exposed by the operating system when the app starts.
- Re-scans automatically every two seconds.
- Provides a manual Refresh action.
- Displays the port name and transport type.
- Displays USB manufacturer, product, and VID/PID metadata when the operating system provides it.
- Handles and displays enumeration errors.
- Does not automatically open any detected port.

### What the statuses mean

A detected port means only that the operating system currently exposes a serial interface. It does **not** prove that:

1. The selected device is the intended Arduino.
2. The application can successfully open the port.
3. The firmware is running or responding.
4. The LoRa radio initialized successfully.
5. Another LoRa device is reachable.

The current UI does not yet perform these checks or provide connected/working/fault states.

## Project structure

```text
messaging-app/
├── frontend/
│   ├── index.html       # App page structure
│   ├── app.js           # Port discovery UI and refresh logic
│   └── styles.css       # Application styling
├── src-tauri/
│   ├── Cargo.toml       # Rust dependencies and package metadata
│   ├── build.rs         # Tauri build integration
│   ├── tauri.conf.json  # Tauri window, frontend, and security config
│   └── src/
│       └── main.rs      # Tauri entry point and list_serial_ports command
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

Cargo downloads the required Rust crates on the first build. Connect or disconnect a USB serial device and observe the list update within the next scan interval, or press **Refresh**.

## Compile an optimized executable

```sh
cargo build --release --manifest-path src-tauri/Cargo.toml
```

The executable is written under:

- **Windows:** `src-tauri/target/release/lora-messaging-app.exe`
- **Linux/macOS:** `src-tauri/target/release/lora-messaging-app`

The current Tauri configuration has bundling disabled (`bundle.active: false`), so this command builds the native executable but does not create an installer or platform package. Build separately on each target operating system; this is not a single universal binary.

## Current backend command

The frontend invokes the Tauri command:

- `list_serial_ports()` — returns the currently enumerated ports, including available USB metadata.

Serial ports are not opened, read, or written by the current backend.

## Planned development sequence

The next steps are intentionally incremental:

1. Add explicit port selection and serial connect/disconnect.
2. Add a defined firmware-to-desktop handshake so the app can distinguish an open port from a responsive device.
3. Add buffered serial reading and writing with clear message framing.
4. Integrate desktop messaging with the existing LoRa messaging protocol.
5. Add meaningful device, firmware, radio, and peer/network status reporting as those checks are implemented.

The desktop serial protocol must be designed together with the Arduino firmware. In particular, firmware debug output and application messages share the serial byte stream, so framing and message separation must be explicit.

## Validation status

The source and project configuration are present in the repository. A successful build, execution on each supported operating system, and tests with physical Arduino/LoRa hardware have **not yet been verified**.
