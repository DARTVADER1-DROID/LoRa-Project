# Campus Assistance Firmware Contracts

**Interface-only architecture scaffold — no operational firmware is implemented.**

This directory translates the Firmware LLD into C++11-compatible public contracts
and separate Arduino Uno / ESP32 station / cart entry points.

## Layout

- `shared/include/ca/`: portable value types, ports, domain contracts, protocol,
  configuration, station use case, UI and role application contracts.
- `shared/include/CampusAssistance.h`: public umbrella include.
- `shared/src/config.cpp`: portable validation for provisioned device, station, destination and RA-02/SX1278 radio settings.
- `shared/src/README.md`: implementation boundary and layering rule.
- `targets/uno/{station,cart}/`: Arduino Uno entry points.
- `targets/esp32/{station,cart}/`: ESP32 entry points; exact board remains TBD.
- `tests/contract_compile.cpp`: host C++11 public-header and API-boundary checks.
- `tests/repository_transition_test.cpp`: fixed-capacity reference tests for atomic transition ordering.
- `tests/config_validator_test.cpp`: configuration validation boundary and malformed-input tests.
- `CMakeLists.txt`: host-side C++11 contract compile target.
- `scripts/`: formatting and format verification commands.

## Build the public contracts

```sh
cmake -S firmware/campus_assistance -B build/campus-assistance
cmake --build build/campus-assistance
ctest --test-dir build/campus-assistance --output-on-failure
```

This compiles the public contracts and shared configuration validator, then runs
configuration and reference transition tests. The fake repository is not a production implementation. It is not an Arduino IDE or PlatformIO
build, and does not verify target memory, timing, radio behavior, flash atomicity,
reboot recovery or hardware adapters.

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

GitHub Actions checks formatting on pull requests and automatically formats firmware
sources on pushes to any branch. The pre-commit hook refuses to touch a partially
staged file with unstaged edits, preventing accidental inclusion of unrelated hunks.

## Architecture rules

- Both role applications use explicit dependency injection.
- Domain and protocol layers contain no Arduino / ESP-IDF dependencies.
- StationClaimCoordinator is the sole assignment-commit authority.
- The request repository exposes only conditional atomic state transitions; it has
  no generic record update that can bypass assignment/cancellation/expiry invariants.
- Claim withdrawal and assignment commit race on the same durable transaction ID;
  a late withdrawal cannot undo a committed assignment.
- ActiveAssignment is the sole owner of cart assignment/journey state; journey
  progression policy is stateless and EventReconciler only orchestrates.
- Protocol events retain their validated envelope metadata and sender-validation
  level for authorization, replay filtering and transaction correlation.
- Retry exhaustion is an explicit outcome, and persistence reads report output length.
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

### Host contract tests

The CMake/CTest suite includes:
- public-header C++11 compile checks and compile-time ownership/API assertions;
- default configuration validation for nonzero identifiers/capacities, wrap-safe request lifetime, bounded registry views, unique station/destination IDs, bounded destination labels and supported SX1278 radio parameter ranges;
- a fixed-capacity reference repository suite for claim-withdraw/commit ordering,
  delayed withdrawal, cancellation/expiry races and expiry-boundary claim rejection.

The reference repository is a test double for the contract, not a production
storage implementation. It does not establish flash atomicity, reboot recovery,
radio behavior or target-MCU correctness.
