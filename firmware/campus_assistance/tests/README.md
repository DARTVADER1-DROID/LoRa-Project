# Host verification status

The host suite validates the portable C++ core and application orchestration using deterministic fake ports. It does not prove electrical behaviour, RF performance, target memory/stack margins, flash endurance or user accessibility.

## Registered suites

CTest currently registers **27 tests**:
- Contract/public-header compilation.
- Shared types, configuration and request identity.
- Pending FIFO, fixed and durable request repositories, allocator rollover/exhaustion.
- Station request lifecycle and station-authoritative claim arbitration.
- Cart claim client, active assignment, driver availability and event reconciliation.
- Protocol frame/payload codecs, endpoint deduplication and retry manager.
- Exception/fault cases for invalid policies, conflicting transactions, capacity, stale/wrong-peer ACKs, cancellation, retry exhaustion and timer rollover.
- Button debounce, gesture interpretation and LCD view formatting.
- Board profile collision checks.
- Application-level station-to-cart workflow integration.

## Application integration test

`application_integration_test.cpp` constructs real `StationApplication` and `CartApplication` objects with the real portable domain components and fake hardware/protocol ports. It exercises:
1. Both application dependency graphs initialize successfully.
2. Station button input creates a station-owned request and queues a REQUEST.
3. Cart accepts the request; unvalidated, spoofed-source and expired requests are rejected.
4. Duplicate request delivery does not duplicate the cart queue entry.
5. Driver explicitly enables acceptance and claims the oldest request.
6. Station rejects a claim whose envelope sender does not match the cart identity.
7. Station arbitration commits the claim and cart applies only an authoritative station assignment.
8. Cart progresses ON_THE_WAY -> ARRIVED_AT_PICKUP -> COMPLETED.
9. Station applies the ordered journey updates and closes its active request.
10. Cart clears terminal assignment only after an authoritative completed snapshot.
11. Both application service loops continue servicing buttons, buzzer, protocol and display ports.

This is a deterministic application-level simulation. It does not simulate RF collisions, randomized packet loss/reordering, target resets, real EEPROM/NVS brownouts or physical peripherals.

## Remaining verification gates

- Target sketches still need real, reviewed provisioning and composition roots; current sketches instantiate adapters but do not construct/service the application objects.
- Add randomized network fault simulation for loss, duplication, reordering and delayed delivery across complete workflows.
- Test cancellation racing with claim/assignment and cart reboot during pending claim/active journey.
- Validate provisioning/identity persistence and multi-station/multi-cart contention.
- Review authentication/replay protection; network-member validation is not cryptographic authentication.
- Validate blind/visually impaired user interaction, particularly destination identification without relying on the LCD.
- Measure Uno SRAM/stack and ESP32 heap/stack/watchdog behaviour with the final target composition.
- Physical RF, electrical, flash endurance, brownout and regulatory checks remain hardware/release gates.

## Exception and retry fault injection

`exception_case_test.cpp` exercises invalid retry policies and transaction IDs, duplicate/conflicting schedule, capacity exhaustion, ACK-before-send, wrong peer and stale sequence ACKs, unknown transaction, cancellation idempotence, rejected transport attempts, retry exhaustion, backoff boundary and `uint32_t` clock rollover.

## Durable station repository and lifecycle

- `durable_request_repository_test.cpp`: snapshot encode/decode, CRC rejection, station identity binding, request-ID continuity, reboot recovery, fail-closed expiry of unassigned work, preservation of committed assignments and simulated interrupted atomic writes.
- `station_request_lifecycle_test.cpp`: enabled-destination validation, request creation/expiry basis, cancellation boundary and unassigned-only expiry.
- The fake persistence adapter tests the old-or-new storage contract; physical flash/EEPROM power-cut testing remains a target integration gate.

A protocol ACK is not proof of physical completion or human awareness.
