# 09 — Verification and Research Plan

## 1. Purpose

This document defines how to verify the prototype and gather evidence for an academic report and research paper. It deliberately separates source-code tests, bench electrical tests, RF experiments, accessibility evaluation and supervised campus trials.

## 2. Build and firmware verification

For every release, record:
- MCU board and revision.
- Arduino core / ESP32 board package version.
- Compiler and build flags.
- LoRa library version.
- LCD library version.
- Firmware version and protocol version.
- Build result for station and cart roles on Uno and ESP32.

Required build matrix:

| MCU | Station firmware | Cart firmware |
|---|---|---|
| Arduino Uno | Compile and bench test | Compile and bench test |
| ESP32 selected board | Compile and bench test | Compile and bench test |

Cross-platform interoperability tests:
- Uno station ↔ Uno cart.
- Uno station ↔ ESP32 cart.
- ESP32 station ↔ Uno cart.
- ESP32 station ↔ ESP32 cart.

All combinations must use compatible protocol and radio configuration.

## 3. Functional test matrix

| Test ID | Scenario | Expected result |
|---|---|---|
| CA-01 | Station boots with valid ID/configuration | Shows ready state and initializes LoRa |
| CA-02 | Duplicate station IDs provisioned | Commissioning audit detects conflict; deployment blocked |
| CA-03 | Student selects valid destination | Destination accepted |
| CA-04 | Student selects unregistered destination | Rejected with audible/visual error |
| CA-05 | Request transmitted | Station reports sent, not confirmed, until ACK |
| CA-06 | Cart receives valid request | Exactly one queue entry created |
| CA-07 | Same request retransmitted | ACK may repeat; no duplicate queue entry |
| CA-08 | Two requests arrive sequentially | Cart preserves FIFO order |
| CA-09 | Queue at capacity | New request rejected; existing queue not overwritten |
| CA-10 | Driver reports Received | Correct request state is updated |
| CA-11 | Driver reports On the way | Correct station receives status |
| CA-12 | Driver reports Arrived at pickup | State distinct from completion |
| CA-13 | Driver reports completion | Request retired only after valid completion handling |
| CA-14 | Student cancels pending request | Cart removes/marks request cancelled |
| CA-15 | Student cancels assigned request | Assigned cart receives cancellation and acknowledges |
| CA-16 | Cancellation and completion cross in flight | Deterministic terminal-state resolution |
| CA-17 | ACK dropped | Sender retries same request identity |
| CA-18 | Cart reboots with queued work | Recovery follows documented persistence policy |
| CA-19 | Station reboots with active request | No silent duplicate dispatch; state reconciled |
| CA-20 | Multiple carts receive same request | Only one cart obtains confirmed ownership |
| CA-21 | No cart is in range | Station reports not confirmed/timeout |
| CA-22 | RF packet has wrong network ID/version/length | Packet rejected |
| CA-23 | Destination registry differs between nodes | Request rejected or explicit config error |
| CA-24 | Two-button UI short/long press | Contextual action occurs exactly once |
| CA-25 | Buzzer patterns in noise | Intended users can distinguish required states |

## 4. RF field-test method

For each test point:
1. Record module variant, antenna, cable/connector and mounting position.
2. Record legal frequency and modem parameters.
3. Record distance and environment.
4. Send a known number of packets in each direction.
5. Count successful receptions and retries.
6. Record RSSI/SNR for each received packet where supported.
7. Record request and status latency.
8. Repeat at different times/occupancy conditions.
9. Report sample count and confidence interval where possible.

Report packet delivery ratio:

`PDR = (successfully received packets / packets transmitted) × 100\%`

Report median and percentile latency, not only the best observed value. Keep station-to-cart and cart-to-station results separate.

## 5. Queue and load experiments

Vary:
- Number of stations.
- Number of carts.
- Request arrival rate.
- Queue capacity.
- Packet retry settings.
- Simultaneous requests.
- Number of carts attempting to claim the same request.

Measure:
- Queue insertion correctness.
- FIFO ordering errors.
- Duplicate request count.
- Queue-full response correctness.
- Claim collision rate.
- Time from request creation to confirmed cart assignment.
- Channel airtime and retry overhead.
- MCU RAM/flash use.

Do not claim support for a device count beyond the largest tested configuration.

## 6. Power and thermal experiments

Measure each node in:
- Boot.
- Idle.
- Continuous receive.
- Transmit burst.
- LCD backlight on/off.
- Buzzer active.
- Typical request/status workload.

Record supply voltage/current, regulator temperature and any resets or packet failures. Use measured values for runtime calculations. If powered from a cart auxiliary rail, test startup transients and electrical noise from the vehicle environment.

## 7. Accessibility evaluation

Plan a supervised usability study with intended users and accessibility stakeholders. Obtain consent and institutional approvals where required.

Evaluate:
- Finding and identifying the two buttons.
- Understanding short and long presses.
- Selecting a destination without seeing the LCD.
- Distinguishing beep patterns.
- Recovering from wrong selection or cancellation.
- Understanding whether a cart has received the request.
- Ease of use and cognitive load.
- Failure recovery when the radio link is unavailable.

Record task completion rate, time, errors, participant feedback and limitations. Do not present informal developer testing as a formal accessibility study.

## 8. Data recording template

For every experiment, record:
- Test ID and objective.
- Date/time and location.
- Hardware revision and MCU type.
- Firmware/protocol version.
- Radio module and antenna.
- RF profile and legal basis.
- Supply voltage/current.
- Test distance and environment.
- Number of attempts and successful receptions.
- RSSI/SNR and latency.
- Queue depth, retries and failures.
- Observations, deviations and corrective action.

## 9. Research-paper structure

A defensible paper can use:
1. Abstract.
2. Introduction and problem motivation.
3. Related work: accessible campus mobility, assistive transport dispatch, LoRa/LPWAN and low-cost embedded systems.
4. System requirements and design methodology.
5. Hardware and firmware architecture.
6. Communication protocol and queue/state logic.
7. Experimental setup.
8. Results: RF, latency, queue correctness, power and usability.
9. Discussion and limitations.
10. Conclusion and future work.

Compare the design against relevant published work using explicit criteria: cost, infrastructure dependency, accessibility interface, request lifecycle, communication range, measured reliability and deployment complexity. Avoid unsupported claims such as “first ever” or “best.”

## 10. Patent preparation

Before public disclosure, discuss the project with the institution's IP cell or patent professional. Prepare:
- Prior-art search log.
- Clear statement of the technical problem.
- Specific technical features believed to be novel.
- System block diagram and sequence diagrams.
- Alternative embodiments.
- Prototype evidence.
- Inventor/contributor records.
- Date-stamped design iterations.

A generic combination of LoRa, a display, buttons and a queue may already be known. Patentability must focus on a genuinely novel technical mechanism or combination, if one exists. Do not claim patentability in project documentation as an established fact.
