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
- `shared/include/ca/protocol/fixed_retry_manager.hpp`: allocation-free exponential retry scheduler with per-transaction outcomes.
- `shared/include/ca/protocol/fixed_protocol_endpoint.hpp`: bounded RX event queue, frame validation, ACK handling and recipient-specific reliable broadcast.
- `shared/include/ca/storage/fixed_cart_registry.hpp`: validated station-side roster for reliable cart broadcasts.
- `shared/include/ca/storage/fixed_station_registry.hpp`: validated cart-side station allowlist.
- `shared/src/application.cpp`: station/cart event-loop orchestration, claim handling, expiry outbox, snapshot query handling and cart-side semantic journey confirmation.
- `platform/include/ca/platform/arduino_core_adapters.hpp`: Arduino GPIO sampling, clock and buzzer adapters.
- `platform/include/ca/platform/arduino_pcf8574_lcd.hpp`: configurable PCF8574-backed 16×2 LCD adapter.
- `platform/include/ca/platform/arduino_sx127x_transport.hpp`: poll-driven raw-SPI SX127x LoRa transport for Arduino targets.
- `shared/src/station/repository_station_claim_coordinator.cpp`: repository-serialized claim/withdraw/assignment transitions.
- `shared/src/station_request_controller.cpp`: station request submission/cancellation and retry-class enqueue.
- `shared/src/README.md`: implementation boundary and layering rule.
- `targets/uno/{station,cart}/`: Arduino Uno entry points.
- `platform/include/ca/platform/esp32/devkit_v1_profile.hpp`: ESP32-WROOM-32 DevKit V1 reference pin profile.
- `platform/include/ca/platform/uno/uno_r3_profile.hpp`: Arduino Uno R3 reference pin profile.
- `targets/esp32/{station,cart}/`: ESP32 entry points; DevKit V1 is the initial reference, not a universal ESP32 target.
- `tests/contract_compile.cpp`: host C++11 public-header and API-boundary checks.
- `tests/repository_transition_test.cpp`: fixed-capacity reference tests for atomic transition ordering.
- `tests/config_validator_test.cpp`: configuration validation boundary and malformed-input tests.
- `tests/types_test.cpp`: wraparound, serial-number ambiguity and state-classification tests.
- `tests/request_identity_test.cpp`: complete RequestId validity, equality and hash tests.
- `tests/pending_request_store_test.cpp`: fixed-capacity FIFO, deduplication, ordering, expiry and local-clear tests.
- `tests/protocol_codec_test.cpp`: binary envelope encoding, CRC, malformed lengths and network validation.
- `tests/station_claim_coordinator_test.cpp`: duplicate claims, competing claimers and withdrawal/commit races.
- `tests/cart_claim_client_test.cpp`: transaction identity, withdrawal and authoritative assignment completion.
- `tests/cart_event_reconciler_test.cpp`: assignment, claim-loss, snapshot and completion reconciliation.
- `tests/fixed_destination_catalog_test.cpp`: bounded destination loading, validation and lookup.
- `CMakeLists.txt`: host-side C++11 contract compile target.
- `platformio.ini`: Arduino-core compile matrix for Uno R3 and ESP32 DevKit V1 station/cart sketches.
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
claim arbitration, cart reconciliation, destination-catalogue, retry-scheduler, protocol-endpoint, button-interpreter and LCD UI tests. The host fake
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
- Reliable broadcasts require a provisioned cart roster and are expanded into per-cart unicast transactions; ACKs are accepted only from the intended recipient.
- Inbound station/cart senders must appear in the corresponding provisioned peer registry; network ID alone is not identity.
- Cart journey updates remain pending at the application layer until a station snapshot confirms the committed event sequence; transport ACK alone does not release the assignment.
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

The ESP32-WROOM-32 DevKit V1 and Arduino Uno R3 have reference pin profiles, but no universal pin map is assumed. No radio library or legal RF profile is frozen. A provisional host-reference frame envelope exists, but its magic, field widths, checksum choice, payload schemas, arbitration timing and retry policy constants remain unapproved pending a separate normative protocol review. Queue capacities and concrete flash persistence layout must be selected from target memory/endurance measurements. RA-02/SX1278 integration
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
cycle under Indian WPC rules. A provisional binary frame envelope is versioned and big-endian
with CRC32; this is a host-reference implementation, not an approved wire standard.
Payload fields are still assembled by application code and need a single normative
message-schema table before interoperability testing. The cart claim client
requires a durable transaction-ID seed. The current station reference policy commits the first valid claim received; it does not provide an RF fairness window. Reliable station broadcasts require a provisioned cart registry; broadcast fan-out and ACK tracking are now implemented in the host reference endpoint. The Arduino core, PCF8574 LCD and raw-SPI SX127x adapters are reference implementations only; none has yet been compiled against the selected target cores or validated on hardware. No physical persistence adapter exists yet.


- The station ledger retains terminal records until capacity pressure; when full,
  insertion may evict the oldest terminal record, never a live or uncertain
  request. This avoids relying on millis() as a wall clock across reboot.
