# Software Validation Report â€” Fresh Host Run

**Date:** 19 November 2026
**Machine:** DESKTOP-LV0O5HM, Windows 11 Pro x64
**Repository:** `E:\Projects\LoRa Firmware\LoRa-Project`
**Branch:** `feature/campus-assistance-firmware-skeleton`
**HEAD:** `6a14771 style: automatically format firmware sources`
**Scope:** Fresh host build, registered host tests, warning-enabled host build, repository hygiene and embedded-build tool availability.

## Executive result

**Host core: PASS for the 26 currently registered tests.**
**Warning-enabled host compile: PASS, with two test-code member-initialization-order warnings.**
**Embedded target rebuild in this run: BLOCKED by the relocated PlatformIO virtual environment.**
**Complete station/cart software: NOT YET VERIFIED; application composition roots remain placeholders.**

This is evidence of a healthy portable core, not proof that every software scenario or complete device workflow is correct.

## Run evidence

### 1. Clean host build

A new build directory was used to avoid relying on previous object files:

`E:\Projects\LoRa Firmware\build\validation-clean`

Commands:
```powershell
cmake -S firmware/campus_assistance -B 'E:\Projects\LoRa Firmware\build\validation-clean' -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build 'E:\Projects\LoRa Firmware\build\validation-clean' --parallel 4
ctest --test-dir 'E:\Projects\LoRa Firmware\build\validation-clean' --output-on-failure
```

- Compiler: MinGW GCC 6.3.0 (`C:/MinGW/bin/c++.exe`)
- Build: **71/71 Ninja steps completed**
- CTest: **26/26 passed, 0 failed**
- CTest elapsed time: **2.18 seconds**
- Exit code: **0**

All registered test names and their PASS results:
1. contract_compile
2. repository_transition_test
3. config_validator_test
4. types_test
5. request_identity_test
6. pending_request_store_test
7. fixed_request_repository_test
8. cart_state_components_test
9. durable_request_repository_test
10. station_request_lifecycle_test
11. protocol_codec_test
12. protocol_payload_codec_test
13. fixed_retry_manager_test
14. exception_case_test
15. fixed_protocol_endpoint_test
16. station_claim_coordinator_test
17. cart_claim_client_test
18. cart_event_reconciler_test
19. fixed_destination_catalog_test
20. station_request_controller_test
21. board_profile_test
22. fixed_button_debouncer_test
23. debounced_button_input_test
24. two_button_event_interpreter_test
25. basic_user_interfaces_test
26. fixed_dual_slot_persistence_test

### 2. Warning-enabled host build

A second independent build used:
`E:\Projects\LoRa Firmware\build\validation-warnings`

Compiler flags: `-Wall -Wextra -Wpedantic`

- Build: **71/71 steps completed**
- Exit code: **0**
- Two warnings, both in test fakes and both `-Wreorder`:
  - `tests/station_request_controller_test.cpp`: `FakeLifecycle` constructor initializer order differs from member declaration order.
  - `tests/basic_user_interfaces_test.cpp`: same issue in `FakeLifecycle`.
- No production-source warnings appeared in the captured output.

These warnings do not currently fail tests, but should be cleaned up to keep warning output useful.

### 3. Repository hygiene and working tree

- `git diff --check`: PASS, exit code 0.
- Branch is `feature/campus-assistance-firmware-skeleton`, tracking origin.
- HEAD is `6a14771`.
- The working tree contains pre-existing uncommitted changes in firmware, tests and docs, plus an untracked DOCX and verification directory. These were preserved; this validation run did not modify source code.

### 4. Embedded target build status

The project defines four PlatformIO environments:
- `uno_station`
- `uno_cart`
- `esp32_station`
- `esp32_cart`

A fresh embedded rebuild could not be run in this session. The PlatformIO virtual environment at `E:\PlatformIO\penv` still references its former base interpreter under `C:\Users\Kesavaram\.platformio\python3\python.exe`, which no longer exists after the environment was relocated. Invoking the moved environment fails with:
`No Python at '"C:\Users\Kesavaram\.platformio\python3\python.exe'`

The standalone interpreter at `E:\PlatformIO\python3\python.exe` is present but does not have the PlatformIO module installed. No attempt was made to reinstall packages or alter the user's toolchain during this validation. Historical 4/4 target-build claims are not counted as a fresh pass in this report.

## What the passing tests do support

The 26 passing tests provide automated evidence for the tested portions of:
- Shared types, configuration validation and request identity.
- Bounded pending queue and repository transitions.
- Station request lifecycle, expiry and cancellation cases.
- Durable repository snapshot/recovery behaviour using host fakes.
- Station claim coordination and cart-side claim/event reconciliation.
- Protocol frame/payload codecs, endpoint behaviour, deduplication and retry management.
- Button debounce/event interpretation and LCD view formatting.
- Dual-slot persistence logic and board-profile consistency.

The exception-case suite includes retry-policy validation, duplicate/conflicting scheduling, capacity exhaustion, invalid/stale/wrong-peer acknowledgements, cancellation, transport rejection, retry exhaustion, backoff deadline and 32-bit clock rollover.

## Important remaining software gaps

The passing count is not a complete scenario-coverage percentage. The current scenario matrix explicitly marks several cases as not yet verified, including:
- Full application composition and service-loop integration for station and cart.
- Complete end-to-end request-to-journey workflow across integrated application objects.
- Request-ID generation rollover policy.
- Cancellation racing with claim/assignment and post-assignment cancellation policy.
- Cart reboot during pending claim or active journey.
- Randomized loss/duplication/reordering/delay simulation across complete workflows.
- Exhaustive expiry-boundary tests.
- Simultaneous button presses and button-held-at-boot behaviour.
- Provisioning persistence and identity lifecycle.
- Target memory/stack/heap/watchdog budgets after final composition.

The target sketch wrappers include role `.ino` entry points, but the platform documentation says the role application composition roots remain placeholders. Thus target compile/link, even when restored, will not by itself prove the whole product workflow.

## Assessment

| Layer | Result |
|---|---|
| Clean portable host build | PASS |
| 26 registered host tests | PASS |
| Warning-enabled compile | PASS with 2 test warnings |
| Git diff whitespace check | PASS |
| Fresh PlatformIO target builds | BLOCKED â€” moved venv broken |
| Full station/cart application integration | NOT VERIFIED |
| Physical hardware and RF | OUT OF SCOPE for this run |

**Conclusion:** The portable software core has a good verified baseline. I would not yet label the overall firmware software-complete or release-ready. The next software work should focus on integrating the actual station/cart application graphs, closing the unverified state-machine and reboot/race scenarios in the scenario matrix, fixing the two test warnings, and restoring PlatformIO in a controlled way before re-running all four target builds.

No hardware or RF operation was performed.
