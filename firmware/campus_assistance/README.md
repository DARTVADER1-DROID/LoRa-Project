# Campus Assistance Firmware Contracts

**Interface-only architecture scaffold — no operational firmware is implemented.**

This directory translates the Firmware LLD into C++11-compatible public contracts
and separate Arduino Uno / ESP32 station / cart entry points.

## Layout

- `shared/include/ca/`: portable value types, ports, domain contracts, protocol,
  configuration, station use case, UI and role application contracts.
- `shared/include/CampusAssistance.h`: public umbrella include.
- `shared/src/README.md`: implementation boundary and layering rule.
- `targets/uno/{station,cart}/`: Arduino Uno entry points.
- `targets/esp32/{station,cart}/`: ESP32 entry points; exact board remains TBD.
- `tests/contract_compile.cpp`: host compiler smoke test for public headers.
- `CMakeLists.txt`: host-side C++11 contract compile target.
- `scripts/`: formatting and format verification commands.

## Build the public contracts

```sh
cmake -S firmware/campus_assistance -B build/campus-assistance
cmake --build build/campus-assistance
ctest --test-dir build/campus-assistance --output-on-failure
```

This checks header consistency only. It is not an Arduino IDE or PlatformIO build,
and does not verify target memory, timing, radio behavior or hardware adapters.

## Format code automatically

See [FORMAT.md](FORMAT.md). Run:

```sh
sh firmware/campus_assistance/scripts/format.sh
```

Enable the local pre-commit hook once per clone:

```sh
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

GitHub Actions independently checks formatting on pull requests.

## Architecture rules

- Both role applications use explicit dependency injection.
- Domain and protocol layers contain no Arduino / ESP-IDF dependencies.
- StationClaimCoordinator is the sole assignment-commit authority.
- The request repository stores complete RequestRecord values and exposes an
  atomic assign-if-unassigned guard.
- Radio transport operates on encoded bytes; applications receive typed events.
- All queues, frame buffers and retry tables must be bounded.
- Cart receive and queue behavior is independent of driver acceptance.
- CLAIM is not ownership; only a matching station-originated ASSIGN activates.
- Disabling acceptance clears only that cart's local pending FIFO.
- Completion forces NOT_ACCEPTING; manual re-enable is required.
- No steering, braking, throttle or traction-control interfaces are permitted.

See the [Firmware LLD](../../docs/campus-assistance/11-firmware-lld/README.md),
especially [SOLID and scalability contracts](../../docs/campus-assistance/11-firmware-lld/06-solid-and-scalability-contracts.md).

## Deliberate open items

No radio library, pin map, final ESP32 board, legal RF profile, numeric wire-field
encoding, queue capacity or persistence layout is frozen. RA-02/SX1278 integration
must retain 3.3 V logic/power requirements and pass Indian WPC verification before
transmission. Hardware adapters and runtime behavior remain future work.
