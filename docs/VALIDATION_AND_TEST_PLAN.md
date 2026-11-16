# LoRa Project — Verification and Test Plan

## 1. Purpose

This plan separates software checks from physical hardware and supervised field testing. A successful build is necessary but does not prove RF reliability or deployment safety.

## 2. Build and static checks

### 2.1 Firmware compile matrix

| Firmware | Target | Required libraries |
|---|---|---|
| Group messaging | Arduino Uno R3 / ATmega328P | SPI, EEPROM, Sandeep Mistry LoRa |
| Campus station | Arduino Uno R3 / ATmega328P, role 1 | SPI, EEPROM, LoRa |
| Campus cart | Arduino Uno R3 / ATmega328P, role 2 | SPI, EEPROM, LoRa, Wire, LiquidCrystal_I2C |

Use reproducible board-core and library versions. Record compiler, core and library versions with test results.

### 2.2 Host-side checks

Run:

    python3 -m unittest discover -s tests -v

Also inspect GitHub Actions results for Arduino compilation and source-contract tests. Host tests do not replace protocol simulation or hardware-in-the-loop testing.

### 2.3 Static review checklist

- [ ] Radio pins match wiring and LoRa.setPins().
- [ ] All nodes use matching RF/modem settings.
- [ ] Network IDs are intentional.
- [ ] Campus station and cart are compiled separately.
- [ ] Every deployed campus device has a unique ID.
- [ ] Frame sizes and compile-time assertions match.
- [ ] Queues, peer tables, caches and buffers remain bounded.
- [ ] Retry loops have finite budgets/timeouts.
- [ ] Delivery status does not claim confirmation without ACK.
- [ ] No vehicle actuation is connected to assistance firmware.

## 3. Bench electrical checks

Use current-limited supplies and appropriate test equipment.

1. Verify exact radio module identity and supported RF band.
2. Measure regulated 3.3 V at the radio during idle, RX and TX.
3. Capture supply droop/transients during TX; verify regulator and wiring margin.
4. Verify Uno 5 V outputs are level-shifted to safe 3.3 V at radio inputs.
5. Verify common ground, SPI integrity, reset behavior and wiring length.
6. Verify antenna connector, antenna band and safe RF load before TX.
7. Check module and regulator temperature during repeated transmissions.
8. Confirm LCD backpack voltage and I2C logic compatibility.

Never transmit without a suitable antenna or RF load.

## 4. Group messaging tests

| ID | Scenario | Expected result |
|---|---|---|
| MSG-01 | Two nodes boot on same network | HELLO discovers peer |
| MSG-02 | Send short message | Receiver prints once; sender confirms if ACK arrives |
| MSG-03 | Send exactly 128 bytes | Accepted without truncation |
| MSG-04 | Send 129 bytes | Entire line rejected |
| MSG-05 | Empty line | Rejected locally |
| MSG-06 | NUL in input | Entire line discarded |
| MSG-07 | Send while prior transaction active | BUSY; no overlapping transaction |
| MSG-08 | Drop first ACK | Sender retries; receiver suppresses duplicate display and re-ACKs |
| MSG-09 | Disable one expected receiver | Partial/unconfirmed result after bounded retries |
| MSG-10 | No known peers | One best-effort broadcast; NO_KNOWN_RECIPIENTS |
| MSG-11 | Inject stale/unexpected ACK | ACK ignored |
| MSG-12 | Reboot a peer | New session epoch observed |
| MSG-13 | More than eight live peers | Peer table overflow diagnostic; no memory overrun |
| MSG-14 | Fill ACK queue | Overflow diagnostic; sender retry recovery exercised |
| MSG-15 | Duplicate device ID | Collision warning; commissioning must resolve |

Record packet attempts, RSSI/SNR if instrumented, retry count, ACK latency, transaction duration and final status.

## 5. Campus assistance tests

### 5.1 Station tests

| ID | Scenario | Expected result |
|---|---|---|
| CAM-S01 | Short press while idle | Request created |
| CAM-S02 | Active request | REQUEST every 5 s |
| CAM-S03 | Hold at least 3 s during request | STUDENT_CANCEL sent; local request closed |
| CAM-S04 | No cart claims | Request expires at 10 min |
| CAM-S05 | Multiple carts claim | One selected by rotating-ID rule |
| CAM-S06 | Selected cart receives ASSIGN | Retry stops after matching ASSIGN_ACK |
| CAM-S07 | ASSIGN_ACK lost | ASSIGN retransmitted every 1.5 s |
| CAM-S08 | Matching heartbeat absent 30 s | Offline-uncertain lock |
| CAM-S09 | Heartbeat returns while locked | Lock remains |
| CAM-S10 | Physical inspection + 5 s hold | Redispatch permitted |
| CAM-S11 | Cancellation ACK lost | Bounded retry; unconfirmed status logged |
| CAM-S12 | Cart reports COMPLETE | Station closes request and ACKs |

### 5.2 Cart tests

| ID | Scenario | Expected result |
|---|---|---|
| CAM-C01 | New REQUEST | Enqueue, beep, delayed REQUEST_ACK |
| CAM-C02 | Duplicate REQUEST | No duplicate queue item; ACK again |
| CAM-C03 | Eleventh pending request | QUEUE_FULL; queue remains bounded |
| CAM-C04 | Press ACCEPT | CLAIM sent; not active until ASSIGN |
| CAM-C05 | Matching ASSIGN | Remove queue item, activate, send ASSIGN_ACK |
| CAM-C06 | ASSIGN targets another cart | Ignore |
| CAM-C07 | Cancelled/expired generation | Tombstone blocks stale work |
| CAM-C08 | Cancel held 3 s while assigned | CART_CANCEL and bounded retry |
| CAM-C09 | COMPLETE while assigned | COMPLETE and bounded retry |
| CAM-C10 | Heartbeat while assigned | Current assignment tuple included |
| CAM-C11 | Fill reply queue | Bounded overflow diagnostic |
| CAM-C12 | Reboot after terminal event | Verify RAM tombstone loss and document recovery |

### 5.3 Fault injection and multi-node tests

- Simultaneous claims from 2, then up to 8 carts.
- Multiple stations and overlapping request IDs.
- Duplicate, delayed, reordered and corrupted frames.
- Independently drop REQUEST_ACK, ASSIGN_ACK, CANCEL_ACK, CART_CANCEL_ACK and COMPLETE_ACK.
- Reset station during request, assignment, cancellation and completion.
- Reset cart while queued and while assigned.
- Test generation and request-ID rollover.
- Saturate queues and verify recovery.
- Sweep interference, distance, obstruction, antenna orientation and node density.

## 6. Desktop application tests

Current main-branch functionality is serial-port discovery and presentation.

- [ ] App starts with no serial device.
- [ ] Connected USB serial device appears.
- [ ] Port list refreshes periodically and on demand.
- [ ] USB metadata appears when exposed by the OS.
- [ ] Device removal is reflected.
- [ ] Non-Arduino serial devices are not falsely labeled as project hardware.
- [ ] UI communicates that port discovery is not firmware/radio handshake.

Do not claim radio connectivity until a firmware handshake is implemented and verified.

## 7. RF and performance measurements

For each configuration, record exact module/board revision, antenna/feedline, center frequency, bandwidth, spreading factor, coding rate, preamble, TX power, legal operating basis, distance, line-of-sight/obstruction, node height, packet attempts, delivery rate, duplicate rate, ACK latency, RSSI/SNR, current consumption in idle/RX/TX, and regulator temperature.

Run repeated trials. Use measured data to choose retry and collision-mitigation values.

## 8. Safety and deployment gates

Do not use the campus prototype for unattended or safety-critical operation until:

1. RF module and regulatory configuration are verified.
2. Electrical interface and supply are validated under TX load.
3. Protocol injection, replay, stale-message and fault behavior are reviewed.
4. Lost-link behavior and operator recovery are rehearsed.
5. Independent human/operational fallback is available.
6. Accessibility review covers LEDs, buzzer, text and controls.
7. Supervised pilot and documented risk assessment are complete.
8. No propulsion, steering, braking or immobilization control is connected.

A heartbeat timeout indicates communication uncertainty. It does not prove the cart stopped, the driver saw a message, or a passenger is safe.

## 9. Test record template

Record test ID/date, firmware commit SHA/build configuration, board/radio/antenna/power details, node IDs/network ID, modem profile and permitted TX settings, environment/distance, steps, expected and actual result, logs/measurements, pass/fail/blocker, and follow-up issue.

Do not mark a test passed without retained evidence such as logs, measurements, screenshots or a linked test record.
