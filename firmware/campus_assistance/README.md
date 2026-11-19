# Campus Assistance Firmware Contracts

**Early implementation stage — not operational firmware.**

This directory translates the Firmware LLD into C++11-compatible public contracts
and separate Arduino Uno / ESP32 station / cart entry points.

## Layout

- `shared/include/ca/`: portable value types, domain contracts, protocol,
  configuration, station use case, UI and role application contracts.
- `shared/include/ca/ports/`: narrow hardware, runtime, persistence and request
  repository abstractions. The root `ports.hpp` is only a convenience aggregate.
- `shared/include/ca/domain/`: one domain contract per responsibility. The root
  `domain.hpp` is only a convenience aggregate.
- `shared/include/ca/cart/`: focused declarations for concrete cart-side policies
  and assignment state.
- `shared/include/ca/storage/`: fixed-capacity storage templates; template bodies
  remain in headers because C++11 requires visible definitions at instantiation.
- `shared/src/cart/`: one implementation file per concrete cart component.
- `shared/include/CampusAssistance.h`: public umbrella include.
- `shared/src/config.cpp`: portable validation for provisioned device, station, destination and RA-02/SX1278 radio settings.
- `shared/src/types.cpp`: shared serial-number ordering, wrap-safe elapsed-time checks and request/journey-state predicates.
- `shared/include/ca/storage/fixed_pending_request_store.hpp`: allocation-free,
  compile-time-bounded cart-local FIFO implementation.
- `shared/include/ca/storage/fixed_request_repository.hpp`: bounded volatile
  station repository template for host/reference use.
- `shared/include/ca/storage/fixed_destination_catalog.hpp`: bounded, allocation-free destination catalogue.
- `shared/include/ca/storage/durable_request_repository.hpp`: bounded,
  versioned/CRC-checked snapshot repository over the atomic persistence port.
- `shared/include/ca/station/station_request_lifecycle.hpp` and
  `shared/src/station/station_request_lifecycle.cpp`: station request creation,
  cancellation, expiry and journey-update use case.
- `shared/src/cart/`: separate implementations for driver availability, active
  assignment, claim ownership and authoritative event reconciliation.
- `shared/src/protocol.cpp`: bounded binary frame codec and structural network validator.
- `shared/src/station/repository_station_claim_coordinator.cpp`: repository-serialized claim/withdraw/assignment transitions.
- `shared/src/station_request_controller.cpp`: station request submission/cancellation and retry-class enqueue.
- `shared/src/README.md`: implementation boundary and layering rule.
- `targets/uno/{station,cart}/`: Arduino Uno entry points.
- `targets/esp32/{station,cart}/`: ESP32 entry points; exact board remains TBD.
- `tests/contract_compile.cpp`: host C++11 public-header and API-boundary checks.
- `tests/repository_transition_test.cpp`: fixed-capacity reference tests for atomic transition ordering.
- `tests/config_validator_test.cpp`: configuration validation boundary and malformed-input tests.
- `tests/types_test.cpp`: wraparound, serial-number ambiguity and state-classification tests.
- `tests/request_identity_test.cpp`: complete RequestId validity, equality and hash tests.
- `tests/pending_request_store_test.cpp`: fixed-capacity FIFO, deduplication, ordering, expiry and local-clear tests.
- `CMakeLists.txt`: host-side C++11 contract compile target.
- `scripts/`: formatting and format verification commands.

## Build the public contracts

```sh
cmake -S firmware/campus_assistance -B build/campus-assistance
cmake --build build/campus-assistance
ctest --test-dir build/campus-assistance --output-on-failure
```

This compiles the public contracts, shared configuration validator and concrete
cart-state components, then runs configuration, request-identity, pending-FIFO,
repository, durable reboot-recovery, station lifecycle, protocol framing,
claim arbitration, cart reconciliation and destination-catalogue tests. The host fake
persistence adapter models the atomic old-or-new contract; it does not prove a
real flash/EEPROM adapter's power-loss behavior. It is not an Arduino IDE or PlatformIO
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

The CMake/CTest suite covers:
- public-header C++11 compilation and compile-time ownership/API assertions;
- RequestId identity, sequence behavior and fixed pending FIFO semantics;
- configuration validation and malformed bounded registry views;
- volatile repository claim/withdraw/commit, cancellation and expiry races;
- durable snapshot recovery, CRC rejection, station identity binding, request-ID
  continuity, interrupted-store rollback and same-object recovery retry;
- reboot fail-closed expiry for unassigned and claim-pending requests while
  preserving committed assignments;
- station lifecycle destination validation, creation, cancellation, journey
  updates and unassigned-only expiry.

The durable repository uses a single versioned, CRC32-protected snapshot. Its host
fake tests the required old-or-new persistence contract; it does not prove that a
physical flash/EEPROM adapter is atomic. A real adapter must use a journal,
dual-slot or equivalent scheme and pass hardware power-cut/endurance testing.

Because the current clock contract is uptime millis rather than a persistent RTC,
the station expires unassigned and claim-pending requests during reboot recovery.
Committed assignments are retained. A persistent wall clock is needed if the
system must preserve the exact remaining ten-minute expiry window across power-off.

The default radio validator accepts the RA-02/SX1278 tuning range and supported
spreading-factor, bandwidth, coding-rate and PA output ranges. This is a module
capability check only; it does not approve a frequency, transmit power or duty
cycle under Indian WPC rules. The binary frame envelope is versioned and big-endian with CRC32; payload fields are still assembled by application code and need a single normative message-schema table before interoperability testing. No concrete flash, radio or MCU adapter exists yet.
