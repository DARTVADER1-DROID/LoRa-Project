# 06 — LoRa Communication Design

## 1. Communication scope

The agreed communication medium is LoRa only. The initial architecture is a custom raw-LoRa application protocol using the radio transceiver directly. It is not LoRaWAN unless deliberately redesigned around a gateway/network server.

No Wi-Fi, Bluetooth, cellular, GPS, internet or cloud link is required for station-to-cart requests.

## 2. Identified radio: Ai-Thinker RA-02 / SX1278

The project will retain the Ai-Thinker RA-02 / SX1278 module, subject to regulatory verification.

**Critical band constraint:** SX1278 modules are commonly supplied for 433 MHz operation and are not 865–867 MHz radios. A firmware frequency constant cannot change the physical RF capability of the chip and matching network.

Before campus transmission:
1. Read exact module marking and datasheet.
2. Confirm supported band and RF matching network.
3. Select a matching antenna.
4. Verify current Indian WPC/DoT requirements for frequency, power/EIRP, bandwidth, duty cycle, channel access and equipment compliance.
5. Record the regulatory source and date in the test report.

The module choice is not regulatory approval.

## 3. Common radio configuration

All stations and carts must use one common LoRa modem/network configuration:
- Frequency.
- Spreading factor.
- Bandwidth.
- Coding rate.
- Preamble length.
- Sync word/network identifier.
- CRC behavior.
- Transmit power, within verified legal limits.

Final values must be selected only after the exact module, regulatory requirements and RF measurements are known. Uno and ESP32 nodes must interoperate using the same over-the-air profile.

## 4. Proposed application message families

The protocol must be compact, versioned and normative. Exact field widths, byte offsets, endianness and enumerations belong in a protocol specification maintained alongside firmware.

| Message | Direction | Purpose |
|---|---|---|
| REQUEST | Origin station → broadcast | Create a new student request |
| REQUEST_ACK | Cart → origin station | Optional receipt/deduplication acknowledgement; does not mean assignment |
| CLAIM | Cart → origin station | Request assignment while driver-enabled and cart idle |
| ASSIGN | Origin station → broadcast/selected cart | Commit the single winning cart |
| ASSIGN_ACK | Winning cart → origin station | Confirm the assignment was stored |
| STATUS | Cart → station | Driver-entered On the way / Arrived at pickup status |
| STATUS_ACK | Station → cart | Acknowledge status processing |
| STUDENT_CANCEL | Origin station → broadcast/assigned cart | Student cancellation event |
| CANCEL_ACK | Cart → station | Confirm cancellation processing |
| COMPLETE | Cart → origin station | Driver confirms drop-off at destination |
| COMPLETE_ACK | Station → cart | Confirm completion processing |
| EXPIRED | Origin station → broadcast | Retire unaccepted request after expiry |
| QUEUE_FULL | Cart → station | Cart cannot store another pending request |
| HEARTBEAT | Cart → network | Optional liveness/diagnostic signal |

A generic event packet with a state field may replace separate message types if the semantics remain unambiguous.

## 5. Request identity and authority

Every request needs a stable identity across retries, preferably:

`(network_id, origin_station_id, request_sequence, generation)`

The origin station:
- Creates the request identity.
- Starts the default 10-minute expiry timer.
- Is authoritative for expiry.
- Commits at most one winning cart for a request.
- Rejects claims received after expiry or after another cart has already won.
- Broadcasts assignment, cancellation and expiry outcomes.

Every state-changing packet includes the full request identity. Do not rely on a sequence alone if it can wrap or restart after power loss.

## 6. Driver-gated claim semantics

- All carts listen to REQUEST messages irrespective of driver mode.
- A cart may store a valid live request in its local FIFO pending list while not accepting.
- A pending-list entry is not an assignment.
- A cart sends CLAIM only when the driver has enabled acceptance and the cart has no active assignment.
- A cart may hold at most one active assignment.
- The station commits one winner and broadcasts ASSIGN.
- Only the cart named in a matching ASSIGN may create the active assignment.
- Other carts remove that request from pending when they receive ASSIGN.
- After completion, the cart remains not accepting until manually enabled again.
- Disabling acceptance clears that cart's current local pending list only; it does not send STUDENT_CANCEL.

## 7. Request expiry and local pending-list cleanup

Default unaccepted-request lifetime: **10 minutes**, configurable in firmware.

- Timer starts at request creation on the origin station.
- Origin station is authoritative and broadcasts EXPIRED.
- Carts remove the matching pending entry when EXPIRED is received.
- A cart may locally age entries as a defensive memory cleanup, but must not override station-confirmed assignment/expiry.
- A late CLAIM is rejected by the station.
- Expiry applies to unaccepted/unassigned requests; an active assignment follows the journey and cancellation state machine.
- Lost EXPIRED/ASSIGN/CANCEL broadcasts require a reconciliation strategy (e.g. bounded rebroadcast, query or tombstone cache); exact strategy remains an open protocol item.

## 8. Airtime, contention and retries

LoRa is a shared, half-duplex medium. Broadcast requests, claims, assignment announcements, acknowledgements, status events and retries all consume airtime.

Record for each packet:
- Payload length.
- SF, BW, CR and preamble.
- Calculated airtime.
- Measured transmit-to-receive latency.
- Retry count and success rate.

Simultaneous claims can collide. The protocol must define bounded randomized backoff or deterministic claim slots, station-side commit behavior, retry limits and recovery after lost ACKs. No cart may assume it won simply because it sent CLAIM.

## 9. RF measurement plan

Record exact module/antenna, placement, legal frequency/profile, distance, environment, packet counts, delivery ratio, retries, RSSI/SNR, latency and supply voltage. Test station-to-cart and cart-to-station directions separately. Repeat with multiple nodes and realistic request/status traffic.

## 10. Security boundary

LoRa CRC detects accidental corruption; it is not encryption or authentication. Device IDs are not proof of identity. Do not transmit student names or sensitive personal data. Document the lack of cryptographic authentication and consider authentication/replay protection as a later design item.

## 11. Regulatory and deployment gate

No field RF deployment is approved by this document. Verify current Indian WPC/DoT rules and equipment conformity for the exact radio and settings. A common 433 MHz RA-02 must not be configured to 865–867 MHz in software.
