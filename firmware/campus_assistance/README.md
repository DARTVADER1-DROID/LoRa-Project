# Campus Assistance Firmware Skeleton

**Interface/stub scaffold only — no operational firmware is implemented.**

This directory translates `docs/campus-assistance/11-firmware-lld/` into organized C++ contracts and Arduino sketch entry points.

## Layout
- `shared/include/ca/`: portable types, hardware ports, domain, protocol and application interfaces.
- `shared/src/`: stub definitions; non-void methods explicitly return NOT_IMPLEMENTED.
- `targets/uno/{station,cart}/`: Arduino Uno sketches.
- `targets/esp32/{station,cart}/`: ESP32 sketches; exact board remains TBD.
- `platform/`: board adapter boundary.
- `tests/`: host-side verification plan.

## Important
This is not yet a complete Arduino IDE/PlatformIO build. No radio library, pin map, RF profile or final ESP32 board has been selected. Sketches are intentionally inert. The next implementation step is to wire the adapters and build system after board and library choices are frozen.

## Non-negotiable behavior
- All carts receive continuously, regardless of driver mode.
- Driver acceptance gates CLAIM only, not receive/queue behavior.
- CLAIM is not ownership; only matching station-originated ASSIGN activates a cart.
- One active assignment maximum per cart.
- Disabling acceptance clears only that cart's local pending list; it does not cancel station requests.
- Requests received after clearing may populate a fresh pending list while disabled.
- Completion forces NOT_ACCEPTING; manual driver re-enable is required.
- Station owns request identity, assignment arbitration, cancellation and 600,000 ms default expiry.
- At most two physical buttons, 16×2 LCD and buzzer.
- No vehicle actuation/control interfaces.

Keep domain independent of Arduino/ESP-IDF. Use bounded storage, non-blocking service loops, explicit errors and common protocol vectors across MCU targets. Follow the LLD implementation order and add host tests before hardware integration.
