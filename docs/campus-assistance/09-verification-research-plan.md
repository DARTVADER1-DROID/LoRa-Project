# 09 — Verification and Research Plan

## 1. Purpose

This document defines verification and evidence collection for the academic prototype. Separate source tests, state-machine tests, electrical tests, RF experiments, accessibility evaluation and supervised campus trials.

## 2. Build and firmware verification

For every release, record MCU board/revision, core/package version, compiler, libraries, firmware version and protocol version.

| MCU | Station firmware | Cart firmware |
|---|---|---|
| Arduino Uno | Compile and bench test | Compile and bench test |
| Selected ESP32 board | Compile and bench test | Compile and bench test |

Cross-platform interoperability:
- Uno station ↔ Uno cart.
- Uno station ↔ ESP32 cart.
- ESP32 station ↔ Uno cart.
- ESP32 station ↔ ESP32 cart.

All nodes must use the same protocol version and LoRa configuration.

## 3. Functional and state-machine test matrix

| Test ID | Scenario | Expected result |
|---|---|---|
| CA-01 | Station boots with valid configuration | Ready state and LoRa initialization reported |
| CA-02 | Duplicate station/cart IDs | Commissioning audit detects conflict; deployment blocked |
| CA-03 | Student selects valid destination | Destination accepted |
| CA-04 | Invalid/unregistered destination | Rejected with feedback |
| CA-05 | Station creates request | Unique ID generated; 10-minute timer starts |
| CA-06 | Cart receives request while not accepting | Request stored once in local pending FIFO; no claim |
| CA-07 | Cart receives request while accepting but already serving | Request stored pending; no second claim |
| CA-08 | Driver enables acceptance while idle | Cart attempts oldest eligible pending request |
| CA-09 | Two pending requests | Oldest eligible request is attempted first |
| CA-10 | Duplicate REQUEST retransmission | No duplicate pending entry |
| CA-11 | Two carts claim same request | Station commits at most one winner |
| CA-12 | Winning ASSIGN received | Winner creates one active assignment |
| CA-13 | Other cart receives ASSIGN | Matching request removed from its pending list |
| CA-14 | Driver disables acceptance | Current local pending list cleared; no student cancellation emitted |
| CA-15 | New request arrives after disable-time clear | Fresh pending entry may be stored; no claim |
| CA-16 | Driver completes journey | Active assignment retired; acceptance remains off |
| CA-17 | Driver manually re-enables after completion | Cart resumes FIFO claim processing |
| CA-18 | Unaccepted request reaches 10-minute deadline | Origin station expires it and broadcasts EXPIRED |
| CA-19 | Claim arrives after expiry | Station rejects late claim |
| CA-20 | Student cancels pending request | All carts remove matching pending entry after cancellation event |
| CA-21 | Student cancels assigned request | Assigned cart receives cancellation and acknowledges |
| CA-22 | Cancellation races with assignment/completion | Deterministic station-authoritative resolution |
| CA-23 | Pending list full | New entry rejected explicitly; existing entries not overwritten |
| CA-24 | Assignment/expiry broadcast is lost | Reconciliation/rebroadcast mechanism eventually removes stale entry; bounded behavior verified |
| CA-25 | ACK dropped | Sender retries same transaction identity |
| CA-26 | Cart reboots with active assignment | Recovery reconciles or enters operator-check state; no silent duplicate dispatch |
| CA-27 | Wrong network ID/version/length | Packet rejected |
| CA-28 | Simultaneous claims collide | No cart assumes assignment without station confirmation |
| CA-29 | Two-button short/long press | Contextual action occurs once; invalid transition rejected |
| CA-30 | Buzzer patterns in campus noise | Intended users can distinguish tested states |

## 4. RF field-test method

At each test point:
1. Record exact module, antenna and mounting.
2. Record legally verified frequency and modem profile.
3. Record distance and environment.
4. Send a known number of packets in each direction.
5. Count successful receptions and retries.
6. Record RSSI/SNR where available.
7. Record request-to-assignment and status latency.
8. Repeat at different occupancy/time conditions.
9. Report sample count and confidence interval where practical.

`PDR = (successfully received packets / packets transmitted) × 100%`

Report median and percentile latency, not only best values. Keep link directions separate.

## 5. Pending-list, assignment and load experiments

Vary:
- Number of stations and carts.
- Request arrival rate.
- Pending-list capacity.
- Number of carts attempting to claim one request.
- Driver acceptance toggles.
- Active assignment duration.
- Retry/backoff settings.
- Packet loss affecting ASSIGN, EXPIRED and CANCEL.

Measure:
- Pending-list insertion and deduplication correctness.
- FIFO ordering per cart.
- Whether any cart holds more than one active assignment.
- Whether non-winning carts remove assigned requests.
- Local queue-clear behavior on disable.
- Whether new requests can be collected while disabled.
- Manual re-enable requirement after completion.
- Claim collision rate and time to confirmed assignment.
- Expiry correctness at 10 minutes and rejection of late claims.
- Stale-entry cleanup after lost broadcast.
- Queue-full behavior, airtime and MCU RAM/flash use.

Do not claim support for a device count beyond the largest tested configuration.

## 6. Power and thermal experiments

Measure boot, idle, continuous receive, transmit burst, LCD backlight and buzzer conditions. Record supply voltage/current, regulator temperature, resets and packet failures. Use measured values for runtime estimates.

## 7. Accessibility evaluation

Conduct supervised evaluation with intended users and relevant accessibility stakeholders, with consent and institutional approval where required.

Evaluate:
- Finding and identifying two buttons.
- Understanding short/long presses.
- Selecting destinations without seeing LCD text.
- Distinguishing audible patterns.
- Understanding request pending versus cart assigned.
- Understanding cancellation, expiry and communication uncertainty.
- Recovering from wrong selection and failed communication.
- Cognitive load and interaction time.

Record task completion, errors, feedback and limitations. Do not present informal developer testing as a formal accessibility study.

## 8. Data recording template

Record test ID, date/location, hardware revision, MCU, firmware/protocol version, radio/antenna, legal RF basis, supply conditions, distance/environment, packet counts, RSSI/SNR, latency, pending-list depth, active assignment, retries, expiry/cancellation outcomes and corrective actions.

## 9. Research-paper structure

1. Abstract.
2. Introduction and problem motivation.
3. Related work: accessible campus mobility, assistive dispatch, LoRa/LPWAN and embedded systems.
4. Requirements and methodology.
5. Hardware and firmware architecture.
6. Driver-gated request assignment, pending-list logic and protocol.
7. Experimental setup.
8. Results: RF, latency, assignment correctness, queue behavior, power and usability.
9. Discussion and limitations.
10. Conclusion and future work.

Compare against published work using explicit criteria. Avoid unsupported claims such as “first ever” or “best”.

## 10. Patent preparation

Before public disclosure, consult the institution's IP cell or a patent professional. Prepare a prior-art search log, technical problem statement, specific potentially novel mechanisms, diagrams, alternative embodiments, prototype evidence and inventor records.

A generic combination of LoRa, LCD, buttons and a queue may already be known. Patentability must be assessed rather than assumed.
