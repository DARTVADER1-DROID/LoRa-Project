# Campus Assistance Firmware â€” Test Run Report

> **Superseded by the latest gap-closure run:** see `SOFTWARE_GAP_CLOSURE_REPORT.md` for the current 27/27 host result, zero strict-build warnings, application integration coverage and current PlatformIO status. This earlier report records the prior 26-test run.

**Run date:** 19 November 2026
**Machine:** DESKTOP-LV0O5HM (Windows 11 Pro x64)
**Repository:** `E:\Projects\LoRa Firmware\LoRa-Project`
**Branch:** `feature/campus-assistance-firmware-skeleton`
**Build configuration:** MSVC Debug / CMake / CTest; PlatformIO embedded builds
**Purpose:** fault-focused verification of the existing firmware core and its four configured target environments.

## 1. Executive result

| Verification layer | Result | Interpretation |
|---|---:|---|
| Host CTest | **26 / 26 passed** | All registered host tests passed |
| New exception-case test | **1 / 1 passed** | Retry manager exception and boundary cases passed |
| PlatformIO targets | **4 / 4 built** | Uno station/cart and ESP32 station/cart compile/link |
| PlatformIO compiler warnings/errors in captured log | **0 matches** | No warning/error strings found in the captured build log |
| `git diff --check` | **Passed** | No whitespace errors in tracked diffs |
| Physical hardware tests | **Not run** | No board/radio/bench measurements were performed |
| End-to-end application integration | **Not verified** | Target sketches remain composition stubs; see limitations |

**Important:** a passing compile is not proof that the station/cart system is operational. The target entry points have not yet been fully wired to the station/cart application graphs. The current target memory figures therefore describe the present compiled target scaffold, not the eventual integrated firmware.

## 2. Host test results

CTest executed all 26 tests successfully. Total reported test time: **1.00 second**.

| # | Test | Result |
|---:|---|---|
| 1 | contract_compile | PASS |
| 2 | repository_transition_test | PASS |
| 3 | config_validator_test | PASS |
| 4 | types_test | PASS |
| 5 | request_identity_test | PASS |
| 6 | pending_request_store_test | PASS |
| 7 | fixed_request_repository_test | PASS |
| 8 | cart_state_components_test | PASS |
| 9 | durable_request_repository_test | PASS |
| 10 | station_request_lifecycle_test | PASS |
| 11 | protocol_codec_test | PASS |
| 12 | protocol_payload_codec_test | PASS |
| 13 | fixed_retry_manager_test | PASS |
| 14 | exception_case_test | PASS |
| 15 | fixed_protocol_endpoint_test | PASS |
| 16 | station_claim_coordinator_test | PASS |
| 17 | cart_claim_client_test | PASS |
| 18 | cart_event_reconciler_test | PASS |
| 19 | fixed_destination_catalog_test | PASS |
| 20 | station_request_controller_test | PASS |
| 21 | board_profile_test | PASS |
| 22 | fixed_button_debouncer_test | PASS |
| 23 | debounced_button_input_test | PASS |
| 24 | two_button_event_interpreter_test | PASS |
| 25 | basic_user_interfaces_test | PASS |
| 26 | fixed_dual_slot_persistence_test | PASS |

Raw log: `verification/host-ctest.log`.

## 3. New exception-case test

Added `tests/exception_case_test.cpp`, registered as CTest test 14. It directly exercises `FixedRetryManager` fault and boundary behaviour:

| Case | Stimulus | Expected / observed |
|---|---|---|
| EX-01 | Retry policy max attempts = 0 | INVALID_ARGUMENT; PASS |
| EX-02 | Initial backoff = 0 | INVALID_ARGUMENT; PASS |
| EX-03 | Maximum backoff below initial backoff | INVALID_ARGUMENT; PASS |
| EX-04 | Backoff above supported signed elapsed-time range | INVALID_ARGUMENT; PASS |
| EX-05 | Transaction ID = 0 | INVALID_ARGUMENT; PASS |
| EX-06 | Schedule same message twice | Idempotent success; PASS |
| EX-07 | Reuse transaction/sequence with altered payload | INVALID_STATE; PASS |
| EX-08 | Fill retry storage and enqueue another item | QUEUE_FULL; existing items retained; PASS |
| EX-09 | ACK before a transmission attempt | STALE_EVENT; PASS |
| EX-10 | Transmission result for unknown transaction/sequence | NOT_FOUND; PASS |
| EX-11 | ACK from wrong device | STALE_EVENT; PASS |
| EX-12 | ACK with stale message sequence | STALE_EVENT; PASS |
| EX-13 | Valid ACK after transmission attempt | ACKNOWLEDGED exactly once; PASS |
| EX-14 | Cancel active delivery twice | First cancellation recorded; second NOT_FOUND; PASS |
| EX-15 | ACK for unknown transaction | NOT_FOUND; PASS |
| EX-16 | Retry transport submission rejected | Attempt still consumed; PASS |
| EX-17 | Poll before retry backoff deadline | No premature retry; PASS |
| EX-18 | Retry limit reached | EXHAUSTED with exactly configured attempts; PASS |
| EX-19 | Millisecond clock wraps at UINT32_MAX | Retry due at correct wrapped deadline; PASS |

The test uses a line-number exit code on assertion failure so a failed MSVC Debug test does not leave an interactive assertion dialog open.

## 4. Embedded target build results

PlatformIO successfully built all four environments:

| Environment | Result | RAM reported | Flash reported |
|---|---|---:|---:|
| `uno_station` | PASS | 980 / 2,048 B (47.9%) | 13,040 / 32,256 B (40.4%) |
| `uno_cart` | PASS | 980 / 2,048 B (47.9%) | 13,040 / 32,256 B (40.4%) |
| `esp32_station` | PASS | 23,384 / 327,680 B (7.1%) | 292,141 / 1,310,720 B (22.3%) |
| `esp32_cart` | PASS | 23,384 / 327,680 B (7.1%) | 292,141 / 1,310,720 B (22.3%) |

Total PlatformIO build time: **43.581 seconds**.

Raw log: `verification/platformio-build.log`.

**Memory caution:** these figures are generated from the currently configured sketches. Because the final application composition is not yet integrated, these are not final firmware memory budgets. The Uno's 2 KB SRAM is particularly constrained; stack high-water measurement and runtime heap/stack checks remain mandatory after integration.

## 5. Scenario coverage and remaining gaps

The companion file `TEST_SCENARIO_MATRIX.md` contains the broader scenario plan across provisioning, request lifecycle, cart queue and assignment, protocol, persistence, user interface/accessibility, hardware and release gates.

The existing host tests provide useful coverage of:
- Request identity and repository state transitions.
- Request expiry, cancellation and assignment lifecycle.
- Station-authoritative claim coordination.
- Cart claim reconciliation and active assignment lifecycle.
- Protocol codec, payload validation, deduplication and retry behaviour.
- Dual-slot persistence, CRC rejection and simulated interrupted writes.
- Button debounce/event interpretation and LCD view formatting.

The following remain **not verified**:
1. Complete station/cart application construction and service-loop integration.
2. Multi-node end-to-end workflow using physical station and cart boards.
3. Actual flash/EEPROM/NVS torn-write and brownout behaviour.
4. RF packet loss, collisions, interference, range, antenna and airtime measurements.
5. Uno SRAM stack high-water under the final application graph.
6. ESP32 watchdog, heap and long-duration service-loop behaviour.
7. Electrical validation of RA-02 supply and logic-level interfacing.
8. Blind/visually impaired user validation of destination selection and feedback.
9. Indian WPC/DoT compliance confirmation for the chosen radio frequency, power, antenna and operating conditions.
10. Authentication/replay protection: network membership validation is not cryptographic authentication.

## 6. Engineering findings and release blockers

- The target sketches still do not construct and service the complete `StationApplication` and `CartApplication` object graphs. The target build therefore validates compile/link compatibility, not working workflows.
- Deployment IDs, network identity, station/cart roster and destination configuration need a reviewed provisioning strategy.
- A blind student's destination selection must not depend on reading the LCD. The one-station-per-fixed-destination approach remains a candidate design decision, not a validated user requirement.
- RA-02/SX1278 433 MHz operation must not be enabled for deployment until Indian regulatory requirements and the actual antenna/power configuration are verified.
- The Uno requires a properly designed 3.3 V supply and logic-level interface for the RA-02; compile success says nothing about electrical safety.

## 7. Reproduction commands

From the firmware directory:

```powershell
# Host
cmake -S . -B 'E:\Projects\LoRa Firmware\build\host-msvc'
cmake --build 'E:\Projects\LoRa Firmware\build\host-msvc' --config Debug --parallel
ctest --test-dir 'E:\Projects\LoRa Firmware\build\host-msvc' -C Debug --output-on-failure

# Embedded targets
pio run
```

The actual host build directory used for this report was:
`E:\Projects\LoRa Firmware\build\host-msvc`.

## 8. Final assessment

**Software-core status: host-verified for the 26 registered tests.**
**Embedded compile status: all four configured targets build successfully.**
**System status: NOT READY for deployment or hardware acceptance.**

This is a test report, not a certification. Do not connect the firmware to a live campus workflow or transmit over the air until application integration, provisioning, electrical checks, RF/regulatory review and accessibility validation are complete.
