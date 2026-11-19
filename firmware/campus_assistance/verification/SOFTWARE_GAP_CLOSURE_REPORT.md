# Software Gap-Closure Report â€” 19 November 2026

**Machine:** DESKTOP-LV0O5HM (Windows 11 Pro x64)
**Repository:** `E:\Projects\LoRa Firmware\LoRa-Project`
**Branch:** `feature/campus-assistance-firmware-skeleton`
**HEAD at start of work:** `6a14771`
**Scope:** close selected portable-software verification gaps, add real application orchestration simulation, remove test warnings, repair moved PlatformIO Python environment and rebuild target matrix.

## Executive summary

| Area | Result |
|---|---|
| Host suite | **27/27 passed** |
| New application workflow integration test | **PASS** |
| Request-ID sequence rollover/exhaustion test | **PASS** |
| Warning-enabled host compile | **PASS; previously observed test warnings corrected** |
| `git diff --check` | **PASS** |
| PlatformIO Python environment | **Repaired; Core 6.1.19 launches via venv Python** |
| Uno station/cart embedded builds | **2/2 passed** |
| ESP32 station/cart embedded builds | **Not completed; dependency installation stalled during Arduino-ESP32 framework setup and was stopped** |
| Target station/cart composition roots | **Still incomplete** |
| Randomized network fault simulation | **Still incomplete** |

This work improves software evidence substantially, but does not make the whole firmware release-ready.

## Changes made

### 1. Added application-level integration test

Added `tests/application_integration_test.cpp` and registered it with CMake/CTest.

The test constructs the real `StationApplication` and `CartApplication`, real portable repositories/controllers/reconcilers/UIs, and deterministic fake hardware/protocol ports. It exercises:

- Both application dependency graphs initialize.
- Station button input creates a request and queues a REQUEST message.
- Cart rejects unvalidated, misattributed and expired request events.
- Duplicate request delivery does not duplicate the cart's pending queue entry.
- Driver explicitly enables acceptance and claims the oldest request.
- Station rejects a CLAIM whose envelope sender does not match the claimed cart.
- Station-authoritative arbitration commits the winner.
- Cart rejects an assignment attributed to the wrong station.
- Cart accepts the station's assignment and enters SERVING.
- Journey progresses through ON_THE_WAY, ARRIVED_AT_PICKUP and COMPLETED.
- Station applies journey updates and closes its active request.
- Cart clears the terminal local assignment only after a completed authoritative snapshot.
- Both application service loops continue servicing injected hardware ports.

**Scope limitation:** protocol events are injected as already-decoded, validated events. The test validates application orchestration and policy, not physical transport, RF scheduling, cryptographic authentication or random packet-loss behaviour.

### 2. Added request identity rollover and exhaustion coverage

Extended `fixed_request_repository_test.cpp` to verify:
- 32-bit sequence rollover increments the generation.
- The final representable sequence/generation is issued once.
- The allocator fails closed with `CAPACITY_EXCEEDED` instead of reusing an identity.

### 3. Added transition-race and persistence interruption coverage

Extended `fixed_request_repository_test.cpp` to exercise both serialized claim/cancel orderings:
- Cancellation commits while a claim is pending; later assignment commit is rejected and the request remains CANCELLED.
- Assignment commits first; later cancellation is rejected as ALREADY_ASSIGNED.

Extended `fixed_dual_slot_persistence_test.cpp` to inject a write interruption at each byte-write budget from 0 through 180, reboot the persistence object, and assert that the recovered value is exactly the old or new committed recordâ€”never a mixed value.

### 4. Removed warning noise from test fakes

Corrected member declaration order in:
- `tests/station_request_controller_test.cpp`
- `tests/basic_user_interfaces_test.cpp`

This addresses the two `-Wreorder` warnings observed in the earlier strict build. The subsequent full strict build output contained no warnings.

### 5. Repaired the relocated PlatformIO virtual environment

The moved `E:\PlatformIO\penv` still referenced its former Python home under C:. Updated `pyvenv.cfg` to use the interpreter now at `E:\PlatformIO\python3\python.exe`.

Verified:
- `E:\PlatformIO\penv\Scripts\python.exe` starts.
- `import platformio` succeeds.
- PlatformIO Core reports version **6.1.19**.

Pinned both ESP32 environments to the already installed, reproducible `espressif32@6.12.0` platform rather than pulling the latest major platform and its unrelated RISC-V toolchain. The `pio.exe` launcher itself did not launch successfully in this moved environment, so builds are invoked reliably with:
```powershell
E:\PlatformIO\penv\Scripts\python.exe -m platformio run --project-dir firmware\campus_assistance -e uno_station -e uno_cart -e esp32_station -e esp32_cart
```

## Fresh host verification

Build directory:
`E:\Projects\LoRa Firmware\build\validation-integration`

Compiler: MinGW GCC 6.3.0
Flags: `-Wall -Wextra -Wpedantic`
Build: **73/73 steps completed**
Compiler warnings: **0** under `-Wall -Wextra -Wpedantic`
CTest: **27/27 passed, 0 failed**
CTest runtime: **2.61 seconds**
Exit code: **0**

The additional 27th test is `application_integration_test`.

The full test list is:
1. contract_compile
2. application_integration_test
3. repository_transition_test
4. config_validator_test
5. types_test
6. request_identity_test
7. pending_request_store_test
8. fixed_request_repository_test
9. cart_state_components_test
10. durable_request_repository_test
11. station_request_lifecycle_test
12. protocol_codec_test
13. protocol_payload_codec_test
14. fixed_retry_manager_test
15. exception_case_test
16. fixed_protocol_endpoint_test
17. station_claim_coordinator_test
18. cart_claim_client_test
19. cart_event_reconciler_test
20. fixed_destination_catalog_test
21. station_request_controller_test
22. board_profile_test
23. fixed_button_debouncer_test
24. debounced_button_input_test
25. two_button_event_interpreter_test
26. basic_user_interfaces_test
27. fixed_dual_slot_persistence_test

`git diff --check`: PASS.

## Remaining software gaps â€” not silently waived

1. **Target composition roots:** Uno and ESP32 station/cart sketches still instantiate peripheral adapters but do not construct and service the real application graphs. Actual deployment IDs, network configuration, station/cart roster, destination catalogue and radio profile require explicit provisioning. I did not invent these values or enable an unapproved RF configuration.
2. **Randomized network simulation:** no seeded simulator yet exercises packet loss, duplication, reordering, long delays, ACK loss and queue pressure across complete workflows.
3. **Cancellation races:** claim/assignment versus student/operator cancellation still needs explicit concurrency/order tests and a documented winner policy.
4. **Cart reboot recovery:** reboot during claim-pending and active journey needs an application-level recovery design and tests, not only component snapshot tests.
5. **Provisioning and identity lifecycle:** configuration persistence, replacement device, duplicate roster IDs and safe re-provisioning need an end-to-end policy.
6. **Security:** sender validation currently proves network membership, not cryptographic authentication or replay protection.
7. **Accessibility:** destination selection still needs a non-visual, user-validated interaction design.
8. **Target runtime budgets:** final application composition is required before meaningful Uno SRAM/stack and ESP32 heap/stack/watchdog budgets can be measured.

## Release interpretation

The portable application orchestration now has a passing host simulation of the full happy path plus several rejection paths. Request identity rollover and the previously reported warning issues are addressed.

Do not call the firmware software-complete yet: the target composition roots, randomized fault simulation, cancellation race policy, reboot recovery and provisioning/security questions remain open. Hardware, RF, regulatory and accessibility gates remain separate.
