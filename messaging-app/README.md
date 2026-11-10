# LoRa Messaging Desktop App

Cross-platform desktop application built with **Rust + Tauri 2**. The frontend is plain HTML, CSS, and JavaScript; no Node.js frontend toolchain or frontend framework is required.

## Current functionality

- Enumerates operating-system serial ports at startup.
- Refreshes the port list every two seconds.
- Supports manual refresh.
- Displays port name, transport type, manufacturer, product, and USB VID/PID.
- Does not open ports automatically.

Port discovery confirms only that the operating system exposes a serial port. It does not establish that the Arduino firmware or LoRa radio is healthy.

## Prerequisites

- Rust toolchain (stable).
- Tauri platform prerequisites:
  - **Windows:** Microsoft C++ Build Tools (MSVC) and WebView2 Runtime.
  - **Linux:** the system WebKitGTK and GTK development packages required by Tauri.
  - **macOS:** Xcode Command Line Tools.

No Qt, Node.js, npm, or separate frontend build step is used.

## Run in development

From this directory:

```sh
cargo run --manifest-path src-tauri/Cargo.toml
```

The first build downloads Rust crate dependencies and compiles the native desktop application.

## Build

```sh
cargo build --release --manifest-path src-tauri/Cargo.toml
```

The release executable is placed under `src-tauri/target/release`.

## Next steps

1. Add explicit serial connect/disconnect controls.
2. Add a versioned firmware handshake and heartbeat.
3. Add buffered serial message reading and writing.
4. Integrate the existing LoRa messaging protocol.
