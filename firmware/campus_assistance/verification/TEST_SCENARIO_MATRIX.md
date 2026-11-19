# Campus Assistance Firmware â€” Verification Scenario Matrix

**Purpose:** define the normal, boundary, fault-injection, recovery, security, accessibility and hardware scenarios required before a campus pilot.

**Status meanings**
- **Automated-pass:** exercised by a passing host test in this repository.
- **New automated-pass:** exercised by `exception_case_test` added for this verification run.
- **Not yet verified:** required, but current tests do not prove it.
- **Hardware gate:** requires the actual target board, radio, power system or user testing.

Host fakes validate software contracts. They do not prove RF behaviour, electrical safety, flash endurance, timing on target silicon, or human accessibility.

## A. Identity, configuration and provisioning

| ID | Scenario / expected result | Verification |
|---|---|---|
| CFG-01 | Valid station configuration accepted | Automated-pass: config_validator_test |
| CFG-02 | Valid cart configuration accepted | Automated-pass: config_validator_test |
| CFG-03 | Zero/invalid device or network identity rejected | Automated-pass: config_validator_test |
| CFG-04 | Unsupported protocol version rejected | Automated-pass: config_validator_test |
| CFG-05 | Invalid radio profile values rejected | Automated-pass: config_validator_test |
| CFG-06 | Invalid capacities / impossible buffer settings rejected | Automated-pass: config_validator_test |
| CFG-07 | Duplicate station IDs rejected | Not yet verified: extend registry validation |
| CFG-08 | Duplicate cart IDs rejected | Not yet verified: extend registry validation |
| CFG-09 | Disabled station/cart cannot be authorized | Automated-pass in validator/registry coverage; recheck full matrix |
| CFG-10 | Empty destination catalogue and unknown destination fail closed | Automated-pass: fixed_destination_catalog_test + lifecycle tests |
| CFG-11 | Wrong network ID / wrong role cannot join | Automated-pass: protocol endpoint / validator coverage |
| CFG-12 | Provisioning survives reboot and cannot silently reuse identity | Not yet verified end-to-end |

## B. Student request lifecycle

| ID | Scenario / expected result | Verification |
|---|---|---|
| REQ-01 | Valid request creates one station-owned request | Automated-pass: lifecycle/repository tests |
| REQ-02 | Duplicate request identity is idempotent/rejected, never duplicated | Automated-pass: repository transition tests |
| REQ-03 | Request ID allocation remains monotonic across reboot | Automated-pass: durable_request_repository_test |
| REQ-04 | Request ID generation/sequence rollover policy | Automated-pass: fixed_request_repository_test (sequence-to-generation rollover and final-ID exhaustion) |
| REQ-05 | Unknown destination rejected | Automated-pass: station_request_lifecycle_test |
| REQ-06 | Disabled destination rejected | Automated-pass: station_request_lifecycle_test |
| REQ-07 | Pending request expires at configured lifetime | Automated-pass: lifecycle/repository tests |
| REQ-08 | Expiry boundary at exactly deadline | Automated-pass: fixed_request_repository_test and station_request_lifecycle_test; exhaustive timing fuzz still not run |
| REQ-09 | Assigned request does not expire as unassigned work | Automated-pass: durable repository/lifecycle tests |
| REQ-10 | Student cancellation before assignment succeeds | Automated-pass: lifecycle/repository tests |
| REQ-11 | Student cancellation after assignment is rejected or follows explicit policy | Not yet verified end-to-end |
| REQ-12 | Station/operator cancellation follows authority rules | Not yet verified end-to-end |
| REQ-13 | Cancellation racing with claim/assignment resolves deterministically | Automated-pass: fixed_request_repository_test covers both serialized orders (cancel wins / assignment wins); concurrent multi-thread execution is not applicable to the single-thread event loop |
| REQ-14 | Request store full reports capacity error and preserves existing entries | Automated-pass: fixed repository tests |
| REQ-15 | Station reboot restores pending/assigned states without ID reuse | Automated-pass in host fake; hardware persistence gate remains |

## C. Cart queue, driver availability and assignment

| ID | Scenario / expected result | Verification |
|---|---|---|
| CRT-01 | Cart boots NOT_ACCEPTING | Automated-pass: cart state component tests |
| CRT-02 | Cart cannot claim while disabled | Automated-pass: claim/state tests |
| CRT-03 | Driver explicitly enables acceptance | Automated-pass: cart state tests |
| CRT-04 | Driver disables acceptance; local pending queue clears only | Automated-pass: pending store/reconciler tests |
| CRT-05 | Local clear does not cancel station-owned student request | Automated-pass in repository/transition tests |
| CRT-06 | One cart cannot hold two active assignments | Automated-pass: active assignment tests |
| CRT-07 | CLAIM alone never activates assignment | Automated-pass: station claim coordinator tests |
| CRT-08 | Competing carts claim same request; one station-authoritative winner | Automated-pass: station_claim_coordinator_test |
| CRT-09 | Losing cart withdraws/clears in-flight claim state | Automated-pass: cart_event_reconciler_test |
| CRT-10 | Claim response lost; snapshot reconciles ownership | Automated-pass: cart_event_reconciler_test |
| CRT-11 | Cart reboots while claim pending | Not yet verified end-to-end |
| CRT-12 | Cart reboots while serving; assignment restored safely | Not yet verified end-to-end |
| CRT-13 | Journey completion forces NOT_ACCEPTING | Automated-pass: cart_event_reconciler_test |
| CRT-14 | Driver must explicitly re-enable after completion | Automated-pass at component level |
| CRT-15 | Cart pending FIFO full / duplicate / local clear | Automated-pass: pending_request_store_test |

## D. Protocol, radio delivery and hostile/stale traffic

| ID | Scenario / expected result | Verification |
|---|---|---|
| NET-01 | Golden frame encode/decode vectors | Automated-pass: protocol_codec_test |
| NET-02 | Truncated, oversized, malformed and CRC-invalid frames rejected | Automated-pass: protocol_codec_test / endpoint tests |
| NET-03 | Payload type/length mismatch rejected | Automated-pass: protocol_payload_codec_test |
| NET-04 | Duplicate message is ACKed but not delivered twice | Automated-pass: fixed_protocol_endpoint_test |
| NET-05 | Same identity with changed payload is rejected | Automated-pass: fixed_protocol_endpoint_test |
| NET-06 | Wrong network/version/source/destination rejected | Automated-pass: endpoint/validator tests |
| NET-07 | ACK from wrong peer rejected | New automated-pass: exception_case_test |
| NET-08 | ACK before any transmission attempt rejected | New automated-pass: exception_case_test |
| NET-09 | ACK with stale/wrong sequence rejected | New automated-pass: exception_case_test |
| NET-10 | Unknown ACK transaction ignored as NOT_FOUND | New automated-pass: exception_case_test |
| NET-11 | Duplicate schedule is idempotent | New automated-pass: exception_case_test |
| NET-12 | Reused transaction/sequence with changed payload rejected | New automated-pass: exception_case_test |
| NET-13 | Retry queue capacity reached without evicting work | New automated-pass: exception_case_test |
| NET-14 | Invalid retry policy / zero transaction rejected | New automated-pass: exception_case_test |
| NET-15 | Exponential backoff obeys configured limits | Automated-pass: fixed_retry_manager_test; expand stress cases |
| NET-16 | Retry exhaustion produces one terminal outcome | New automated-pass: exception_case_test |
| NET-17 | Cancelled retry produces one terminal outcome | New automated-pass: exception_case_test |
| NET-18 | Timer wraparound preserves retry deadline | New automated-pass: exception_case_test |
| NET-19 | Packet loss, duplication, reordering and long delay across full workflows | Not yet verified with randomized network simulator; app integration test covers duplicate request delivery and source validation only |
| NET-20 | Radio unavailable at startup / runtime recovery | Automated-pass at fake endpoint level; hardware gate remains |
| NET-21 | RF collision, interference, hidden-node and range limits | Hardware gate |
| NET-22 | Airtime/duty-cycle/channel access and legal operating band | Hardware/regulatory gate |
| NET-23 | Replay/spoofing resistance and cryptographic authentication | Not implemented / security review required |

## E. Persistence, corruption and recovery

| ID | Scenario / expected result | Verification |
|---|---|---|
| STO-01 | Empty store initializes safely | Automated-pass: fixed_dual_slot_persistence_test |
| STO-02 | Valid record survives reload | Automated-pass |
| STO-03 | Interrupted first write is detected; no false empty state | Automated-pass |
| STO-04 | Torn update preserves previous committed slot | Automated-pass: fixed_dual_slot_persistence_test now sweeps write interruption budgets 0â€“180 and verifies old-or-new recovery after reinitialization |
| STO-05 | CRC corruption fails closed | Automated-pass |
| STO-06 | Invalid slot marker fails closed | Automated-pass |
| STO-07 | Wrong station/network identity cannot load another ledger | Automated-pass: durable repository test |
| STO-08 | Storage write failure does not publish uncommitted state | Automated-pass in host fake |
| STO-09 | Reboot restores request ID allocator and assignment | Automated-pass in host fake |
| STO-10 | Actual EEPROM/NVS/flash power cut at every write boundary | Hardware gate |
| STO-11 | Flash wear/endurance and write amplification | Hardware gate |
| STO-12 | Low-voltage brownout during commit and recovery | Hardware gate |

## F. Buttons, LCD, buzzer and accessibility

| ID | Scenario / expected result | Verification |
|---|---|---|
| UX-01 | Button bounce is filtered | Automated-pass: fixed_button_debouncer_test |
| UX-02 | Short press and long press distinguished | Automated-pass: two_button_event_interpreter_test |
| UX-03 | Simultaneous button press / invalid button index handled | Not yet verified comprehensively |
| UX-04 | Button held during boot does not trigger unintended action | Not yet verified |
| UX-05 | LCD line width and truncation are safe | Automated-pass: basic_user_interfaces_test |
| UX-06 | Selected destination marker does not overwrite label | Automated-pass: basic_user_interfaces_test |
| UX-07 | Buzzer patterns map to actionable states | Not yet verified with hardware |
| UX-08 | Blind student can identify and confirm intended destination without LCD | Hardware/user validation gate; current interaction design unresolved |
| UX-09 | Ambiguous, interrupted or repeated input does not submit duplicate request | Not yet verified end-to-end |
| UX-10 | Power-on, fault, request, assignment, arrival and completion feedback is distinguishable | Hardware/user validation gate |

## G. Hardware, timing, safety and system integration

| ID | Scenario / expected result | Verification |
|---|---|---|
| HW-01 | Uno compile and link for station/cart | PlatformIO build gate; run logged separately |
| HW-02 | ESP32 DevKit V1 compile and link for station/cart | PlatformIO build gate; run logged separately |
| HW-03 | SRAM/flash/stack budget on Uno with final composition | Not yet verified |
| HW-04 | ESP32 heap/stack/watchdog budget | Not yet verified |
| HW-05 | Non-blocking service-loop worst-case latency | Not yet verified |
| HW-06 | RA-02 3.3 V supply, logic levels, current peaks and decoupling | Hardware electrical gate |
| HW-07 | Antenna, RF output, conducted/radiated emissions and range | Hardware/regulatory gate |
| HW-08 | Indian WPC/DoT authorization and permitted frequency/power/duty cycle | Regulatory gate |
| HW-09 | Battery low, brownout, reset and watchdog recovery | Hardware gate |
| HW-10 | LCD, buzzer, buttons, radio and storage fail independently and report fault | Not yet verified end-to-end |
| HW-11 | Full station-to-cart journey with two physical nodes | Hardware gate; host application_integration_test now exercises the complete workflow with fake ports |
| HW-12 | Multiple stations and carts, simultaneous requests and load soak | Not yet verified |
| HW-13 | Thermal, ESD, reverse polarity, short-circuit and power sequencing | Hardware safety gate |
| HW-14 | Accessibility testing with intended users in realistic campus conditions | User validation gate |

## Release gates

A host pass is necessary but insufficient. Do not mark firmware production-ready until:
1. All target entry points instantiate and service the real station/cart application graph (still outstanding; host application composition is covered with fakes).
2. Every software scenario marked not-yet-verified is implemented or explicitly waived with rationale.
3. All four target builds pass with memory reports and warnings reviewed.
4. Hardware power, pin-level, radio and persistence tests pass on the exact BOM.
5. Indian regulatory compliance is documented before any RF transmission.
6. Blind/visually impaired users validate the request-selection and feedback flow.
7. A multi-node soak test demonstrates recovery from loss, duplicates, reboots and brownouts.
