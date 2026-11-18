# 07 — Firmware and Data Model

## 1. Firmware organization

Target MCU families:
- Arduino Uno / ATmega328P.
- ESP32 (exact board variant remains to be selected).

Each requires a dedicated hardware-specific build. Both must implement the same over-the-air packet schema, protocol version, RF profile and state semantics. A station on Uno must interoperate with a cart on ESP32 and vice versa.

Suggested logical modules:
- Common protocol and request identity.
- Common request lifecycle and assignment state machine.
- Common pending-list/active-assignment logic.
- Common button event abstraction.
- Board-specific radio, LCD, buzzer, GPIO and nonvolatile-storage drivers.
- Role-specific station and cart application logic.

Do not restructure the existing source until its build workflow and tests have been reviewed.

## 2. Compile-time configuration

Each node is configured with:
- DEVICE_ROLE: STATION or CART.
- DEVICE_ID: unique numeric identifier.
- DEVICE_LABEL / LOCATION_LABEL.
- NETWORK_ID.
- PROTOCOL_VERSION.
- Common RF/modem profile.
- Queue/pending-list capacity.
- Request expiry duration (default 10 minutes).
- Board-specific pin map and peripheral configuration.

Use unique IDs; never flash a shared template ID onto multiple deployed nodes.

## 3. Firmware-level registration

Initial provisioning remains firmware-only:
- No on-device registration menu.
- No password-entry UI.
- No over-the-air enrollment.
- Administrator edits configuration, compiles and flashes the device.
- Commissioning records role, ID, location, network and firmware/protocol versions.

The station registry stores numeric station ID, short label, category and enabled state. Its maximum size must be bounded according to MCU memory. The exact ESP32 board and Uno memory budget must be considered before selecting limits.

## 4. Request identity and record

A request record should contain at least:

| Field | Purpose |
|---|---|
| origin_station_id | Station that created the request |
| request_sequence | Station-local sequence |
| generation/session | Distinguishes reused sequence values after reset/wrap |
| destination_station_id | Registered destination |
| created_at / expiry basis | Origin station's creation time or monotonic elapsed-time basis |
| state | Current request/assignment state |
| assigned_cart_id | None until station commits a winner |
| last_event_sequence | Rejects stale status/cancel events where required |

Do not include student names or sensitive personal data in broadcast packets.

## 5. Two separate cart data structures

Do not model every received request as a service-queue item.

### 5.1 Pending unaccepted list

A bounded FIFO list of live, unassigned requests heard by the cart. Entries are only candidates for future assignment.

- Receiving a request may add it once, whether the cart is accepting or not.
- Pending entries do not mean the cart has accepted responsibility.
- Entries are deduplicated by the full request identity.
- Entries are removed when cancelled, expired or assigned to any cart.
- If the driver disables acceptance, clear the entire local pending list.
- Requests heard after that clear may populate a new pending list while acceptance remains disabled.
- Queue-full must be reported explicitly; never overwrite the oldest live entry.

### 5.2 Active assignment

A cart may hold **zero or one** accepted assignment.

- It is created only after the originating station commits and confirms the cart's claim.
- While an active assignment exists, the cart cannot claim another request.
- It remains active through Received, On the way, Arrived at pickup and completion.
- Completion is driver-confirmed after drop-off.
- After completion, the cart enters NOT_ACCEPTING and requires a new manual enable action.

## 6. Driver availability state machine

Suggested cart modes:
- NOT_ACCEPTING: radio receiver remains active; requests may enter a fresh pending list; no claims.
- ACCEPTING_IDLE: driver has enabled acceptance; no active assignment; claim oldest eligible pending request.
- CLAIMING: one request claim/assignment transaction in progress; do not initiate a second claim.
- SERVING: one active assignment exists; do not claim another request.
- DISABLE_CLEAR: driver disables acceptance; immediately clear the current local pending list and return to NOT_ACCEPTING.
- COMPLETION_LOCKOUT: after journey completion, acceptance remains off until the driver explicitly enables it.

Acceptance mode is persistent only until disabled or an assignment is committed. It does not automatically resume after journey completion.

## 7. Claim and assignment algorithm

1. Receive frame; validate CRC, network ID, protocol version, frame length and message type.
2. Validate request fields and full request identity.
3. Reject stale, cancelled, expired or malformed requests.
4. Deduplicate; add a new valid unassigned request to the local pending FIFO.
5. If driver mode is not ACCEPTING_IDLE, stop here.
6. If idle and accepting, inspect the oldest eligible pending entry.
7. Send CLAIM with request identity and cart ID.
8. Wait for station-originated ASSIGN confirmation, bounded by timeout/retry policy.
9. Only the matching confirmed winner creates the active assignment.
10. Remove the assigned request from its own pending list.
11. Other carts remove it when they receive the assignment announcement.
12. Do not claim another request until active assignment is terminal and the driver manually re-enables acceptance after completion.

The station must commit at most one winner per request. Simultaneous claims, collision backoff, claim expiration and recovery after lost assignment ACKs need normative protocol rules and tests. A cart must never infer victory from its own transmitted CLAIM.

## 8. Request expiry

Default pending-request lifetime is **10 minutes**, configurable by firmware setting.

- Timer starts when the originating station creates the request.
- Origin station is authoritative for expiry.
- Station broadcasts an EXPIRED event containing the full request identity.
- Carts remove matching pending entries on receipt.
- Carts may locally age entries to save memory, but the local timer is advisory if station time/authority is available.
- Station rejects a CLAIM that arrives after the expiry deadline.
- Expiry affects unaccepted/unassigned requests only; an assigned active journey is governed by active-state and cancellation rules.
- Use wrap-safe elapsed-time arithmetic for MCU uptime counters. Do not compare raw timestamps across unsynchronized devices.

## 9. Request lifecycle states

Recommended states:
- CREATED
- PENDING_UNASSIGNED
- CLAIM_PENDING
- ASSIGNED / RECEIVED
- ON_THE_WAY
- ARRIVED_AT_PICKUP
- IN_JOURNEY (optional internal)
- COMPLETED
- CANCELLED
- EXPIRED
- COMMUNICATION_UNCERTAIN

Normal sequence:
`CREATED → PENDING_UNASSIGNED → CLAIM_PENDING → ASSIGNED → ON_THE_WAY → ARRIVED_AT_PICKUP → COMPLETED`

Terminal alternatives: CANCELLED or EXPIRED. Do not allow late packets to overwrite terminal states.

## 10. Two-button event processing

Use a non-blocking button state machine:
- Sample input.
- Debounce.
- Detect press edge.
- Measure press duration.
- Emit SHORT_PRESS or LONG_PRESS.
- Interpret the event according to current UI/cart state.
- Reject invalid transitions and provide clear feedback.

Button meanings are context-sensitive. Maximum two physical buttons is a fixed requirement; exact cart mapping remains an open interface-design item. Avoid long blocking delays that prevent radio servicing.

## 11. Reliability, retries and persistence

Make these configurable and bounded:
- Request broadcast/retry interval.
- Claim response window and collision backoff.
- Assignment confirmation timeout.
- ACK timeout and retry count.
- Request lifetime (default 10 minutes).
- Queue/pending-list capacity.
- Button debounce and long-press threshold.

Use idempotent messages and full request identity in all state-changing events. On reboot, reconcile an active assignment before permitting new claims. EEPROM/flash writes must be minimized; define atomic recovery behavior before relying on persistence.

## 12. Diagnostics

Development serial diagnostics should include:
- Firmware/protocol version, role and device ID.
- Network ID and RF profile.
- LoRa initialization result.
- Driver acceptance mode.
- Active assignment identity (zero or one).
- Pending-list depth and oldest request age.
- Claim/assignment outcomes and retry counters.
- Expiry/cancellation removals.
- RSSI/SNR where available.

Serial diagnostics are for maintainers and do not replace the student/driver LCD and buzzer interface.
