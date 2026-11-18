# 05 — Memory, Portability and Verification

## 1. Portability objective

Uno and the eventual ESP32 implement identical domain semantics and protocol via separate hardware adapters. ESP32 board remains tentative; no pin map or module-specific assumptions are frozen.

## 2. Platform adapter contract

Each target supplies:
- monotonic millisecond clock;
- GPIO/button sampling;
- SPI and SX1278 transport;
- LCD transport;
- buzzer output;
- nonvolatile storage;
- watchdog/health integration where available.

Arduino-core/ESP-IDF types must not leak into domain headers.

## 3. Bounded-memory rules

Mandatory:
- fixed maximum frame size;
- fixed pending capacity per build;
- fixed station/destination registry capacity;
- bounded RX/TX queues and retry table;
- bounded diagnostic ring buffer;
- no unbounded string concatenation or recursion;
- no hidden heap allocation in receive/state-transition paths;
- queue overflow returns explicit error and preserves live entries.

### Uno budget

Before choosing capacities, measure:
- static globals/object instances;
- RX/TX and decoded-frame buffers;
- pending entries × capacity;
- station/destination registry;
- request/assignment records;
- UI text/state;
- retry transaction table;
- stack high-water estimate and safety margin.

ATmega328P RAM is constrained. Use actual sizeof measurements and build/map output; do not choose capacity by guess.

### ESP32 budget

After exact board selection, document available RAM/flash, partitions, driver support, scheduling/thread safety and pin/logic compatibility. The same public contracts and protocol vectors must pass.

## 4. Persistence and reboot

Before implementation decide:
- station ledger persistence;
- generation/session ID persistence;
- cart active assignment recovery;
- interrupted-write detection;
- storage endurance and write frequency.

Safe defaults:
- rebooted cart starts NOT_ACCEPTING;
- potentially active assignment is reconciled before new claims;
- station never issues a second winner for an already committed RequestId;
- storage failure is explicit degraded/uncertain state.

## 5. Timing configuration

Document units and valid ranges for button debounce/long press, radio service cadence, request rebroadcast, claim arbitration, ACK/assignment timeout, retry/backoff, reconciliation and LCD refresh. Request lifetime defaults to 600,000 ms. Use wrap-safe monotonic elapsed time; never compare unsynchronized wall clocks. No blocking delay may starve RF service.

## 6. Diagnostics

Expose firmware/protocol version, role/ID, network/radio profile identifiers, radio health, driver mode, active RequestId, pending depth/oldest age, claim outcomes, duplicate/invalid/expired/cancelled/assigned removals, queue-full, retry exhaustion, reconciliation and RSSI/SNR when available. Do not log personal data. Diagnostics are bounded and non-blocking.

## 7. Host-side test seams

Inject interfaces and provide:
- FakeRadioTransport: drop, duplicate, reorder, delay frames;
- FakeClock: expiry and counter rollover;
- FakePersistence: reboot, corruption, write failure;
- FakeButtonInput: bounce, short/long press;
- FakeDisplay/FakeBuzzer: view model and cue assertions;
- fixed-capacity fixtures for Uno-like limits.

## 8. Required test suites

### Identity and codec
- valid/invalid RequestId; sequence wrap/generation;
- encode/decode golden vectors;
- truncated/oversized/malformed/version mismatch;
- integrity failure;
- Uno/ESP32 interoperability.

### Pending list
- FIFO and deduplication;
- capacity boundary/overflow;
- full-identity removal;
- local clear affects only one cart;
- requests after clear queue while NOT_ACCEPTING;
- assignment/cancel/expiry remove matching item only.

### Driver and assignment
- boot NOT_ACCEPTING;
- no claim while disabled;
- enable claims oldest eligible;
- one claim in flight;
- one active assignment maximum;
- disable clears local pending only;
- completion forces NOT_ACCEPTING;
- no automatic re-enable.

### Distributed cases
- multiple simultaneous carts;
- exactly one station winner;
- duplicate claims;
- lost ASSIGN/ACK and delayed duplicates;
- missed broadcast then reconciliation;
- station reboot with committed assignment;
- CANCEL vs CLAIM race;
- expiry vs CLAIM at 10-minute boundary;
- ambiguous link never yields two winners.

### Time, journey and interface
- default 600,000 ms and configured values;
- before/at/after expiry; millis rollover; clock disagreement;
- assigned request not pending-expired;
- valid journey progression and stale status;
- ACK does not imply physical completion;
- max two buttons; bounce/short/long press;
- 16×2 truncation, buzzer patterns and accessibility user tests.

### Hardware/system
- radio init failure;
- Uno logic-level and power integrity;
- RA-02 3.3 V transient supply;
- LCD logic compatibility;
- no traction-control connection;
- RF range/PDR/RSSI/SNR/airtime/collisions/duty-cycle;
- current, thermal and battery measurements.

## 9. Implementation order

1. Freeze public value types, identity and configuration.
2. Implement/test RequestIdentity and PendingRequestStore on host.
3. Implement/test DriverAvailabilityController and ActiveAssignment.
4. Implement station RequestLifecycle/RequestLedger.
5. Freeze wire schema and golden vectors.
6. Implement/test FrameCodec and MessageValidator on host.
7. Simulate claim, assignment and reconciliation with fault injection.
8. Implement button events and UI view models.
9. Implement Uno adapters and measure memory.
10. Resolve ESP32 board; implement its adapters.
11. Run mixed-MCU interoperability.
12. Conduct supervised RF, power, accessibility and fault-injection tests.

## 10. Module definition of done

A module is integration-ready only when interfaces/invariants and errors are documented, tests cover normal/boundary/failure cases, memory/time bounds are measured, dependencies obey layers, both MCU targets retain protocol compatibility, and docs/test vectors identify the code revision.
