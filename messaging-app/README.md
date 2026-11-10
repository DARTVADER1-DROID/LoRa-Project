# LoRa Messaging Desktop App

Cross-platform desktop application built with **Rust + Tauri 2**. The frontend uses plain HTML, CSS, and JavaScript.

## Current functionality on main

- Discovers operating-system serial ports at startup.
- Refreshes the port list every two seconds and on demand.
- Displays port name, transport type, manufacturer, product, and USB VID/PID when supplied by the operating system.
- Does not open ports automatically.

Port discovery only confirms that the operating system exposes a serial port. It does not identify the connected device conclusively or verify Arduino firmware or LoRa radio health.

The separate `feature/serial-connect-status` branch adds port selection and connect/disconnect controls. See that branch's README for its implementation details.

## Technology

- **Backend:** Rust
- **Desktop shell:** Tauri 2
- **Frontend:** plain HTML, CSS, and JavaScript
- **Serial discovery:** Rust `serialport` crate
- **Frontend framework/build tool:** none

No Qt, Node.js, npm, React, or separate frontend build step is required. Tauri uses the platform's WebView.

## Project structure

```text
messaging-app/
├── frontend/
│   ├── index.html
│   ├── app.js
│   └── styles.css
└── src-tauri/
    ├── Cargo.toml
    ├── build.rs
    ├── tauri.conf.json
    ├── icons/
    │   └── icon.ico
    └── src/
        └── main.rs
```

## Requirements

Install the Rust stable toolchain and the platform-specific Tauri prerequisites:

- **Windows:** Microsoft C++ Build Tools (MSVC) and WebView2 Runtime.
- **Linux:** GTK and WebKitGTK development packages required by Tauri.
- **macOS:** Xcode Command Line Tools.

## Run in development

From this directory:

```sh
cargo run --manifest-path src-tauri/Cargo.toml
```

## Build an optimized executable

```sh
cargo build --release --manifest-path src-tauri/Cargo.toml
```

The executable is written under `src-tauri/target/release`. The current Tauri configuration has bundling disabled, so this command does not create an installer. Build separately on each target operating system.

## Validation status

The repository source has not yet been verified by a successful Rust/Tauri build on the supported operating systems or by tests with physical Arduino/LoRa hardware.
