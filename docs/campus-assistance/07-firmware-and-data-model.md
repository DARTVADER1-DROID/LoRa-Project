# 07 — Firmware and Data Model

## 1. Firmware organization

The project targets two MCU families:
- Arduino Uno / ATmega328P.
- ESP32 (exact board variant to be selected and recorded).

Each MCU requires a dedicated build/configuration. The over-the-air protocol should be common so that a station on Uno can communicate with a cart on ESP32, and vice versa, provided both run compatible protocol and RF settings.

Recommended source organization (target structure; adapt to repository layout):
```
firmware/campus_assistance/
  common/
    protocol.h
    request_state.h
    queue_logic.h
    identity_config.h
  uno/
    station/
    cart/
    board_config.h
  esp32/
    station/
    cart/
    board_config.h
```

This is a proposed organization. Do not move or rewrite existing source until its build workflow and tests are reviewed.

## 2. Compile-time configuration

Each physical node receives a unique configuration:
- DEVICE_ROLE: STATION or CART.
- DEVICE_ID: unique numeric identifier.
- DEVICE_LABEL: optional human-readable label.
- LOCATION_ID / LOCATION_LABEL: station identity and display label.
- NETWORK_ID: campus network identifier.
- PROTOCOL_VERSION.
- RF frequency and modem profile.
- QUEUE_CAPACITY.
- Board-specific GPIO and peripheral configuration.

Use examples such as ST001 and CRT001 in documentation, but transmit compact numeric IDs where practical. Do not deploy the template ID unchanged to multiple devices.

## 3. Firmware-level registration

Initial registration is intentionally simple:
- No on-device registration menu.
- No password-entry UI.
- No over-the-air enrollment.
- Administrator edits the firmware configuration, compiles and flashes the node.
- Unique IDs and station registry entries are checked in a commissioning sheet.
- All devices participating in a network must use a compatible network ID, protocol version and RF profile.

Interface-based registration can be a future feature; it is not part of the initial prototype.

## 4. Station registry

The station registry contains:
- Numeric station ID.
- Short display label (maximum length determined by LCD layout).
- Optional category (library, mess, canteen, department).
- Enabled/disabled flag.

Every station's own location must be represented. The list of destinations shown to a student should include all enabled registered locations except the current pickup location, unless same-station requests are intentionally allowed.

A station count of zero is a valid uncommissioned configuration state. In practice, a working request demonstration requires at least one station and one cart, and destination selection between distinct locations requires at least two registered station locations.

Avoid fixed-size arrays that are too small for expected campus growth. Still use a documented maximum bounded by MCU memory. For Uno, estimate RAM before selecting the registry limit.

## 5. Request data model

Minimum logical request record:

| Field | Meaning |
|---|---|
| origin_station_id | Station where the student requested pickup |
| destination_station_id | Registered destination |
| request_sequence | Monotonic or safely wrapped station-local sequence |
| generation/session | Distinguishes reused IDs after reset/wrap |
| state | Current request lifecycle state |
| assigned_cart_id | Cart selected to service request; zero/none until assigned |
| created_at_local | Optional local timestamp/uptime for diagnostics only |
| flags | Reserved/optional fields |

Do not transmit personally identifying student information. The request should refer to the station and destination, not the student's name.

## 6. Queue implementation

Each cart maintains:
- One active request (head of queue).
- A bounded FIFO pending queue.
- A bounded duplicate cache or request-state table.
- Optional terminal/tombstone records for recently completed/cancelled requests.

Rules:
1. Validate packet and destination before enqueue.
2. Deduplicate using the full request identity.
3. Preserve arrival order of accepted unique requests.
4. Do not overwrite the head when full.
5. Send QUEUE_FULL when capacity is exhausted.
6. Remove/retire only after cancellation or completion has been resolved.
7. On reboot, reconcile persisted active request state before accepting a duplicate as new.

A simple ring buffer is suitable. Queue capacity should be selected independently for Uno and ESP32 after memory profiling; protocol behavior must still report queue-full consistently.

## 7. State transitions

Primary states:
- PENDING_DISPATCH
- RECEIVED
- ON_THE_WAY
- ARRIVED_AT_PICKUP
- COMPLETED
- CANCELLED
- EXPIRED
- COMMUNICATION_UNCERTAIN

Allowed normal sequence:
`PENDING_DISPATCH → RECEIVED → ON_THE_WAY → ARRIVED_AT_PICKUP → COMPLETED`

Do not permit arbitrary forward jumps. Duplicate status messages are idempotent. A late packet from an old generation must not change a newer request state.

Cancellation is allowed only from states where the operational policy permits it. Completion and cancellation are terminal outcomes; conflict resolution must be deterministic.

## 8. Button event processing

Implement button handling as a non-blocking state machine:
- Sample input.
- Debounce.
- Detect press edge.
- Measure press duration.
- Emit SHORT_PRESS or LONG_PRESS event.
- Let the current UI state decide what the event means.
- Ignore invalid actions and provide a short error beep.

Avoid long `delay()` calls for UI or radio timing. The main loop must continue servicing radio packets and button events.

## 9. Timing and retry policy

All intervals must be configurable and measured:
- Request broadcast/retry interval.
- Claim/assignment response window.
- ACK timeout.
- Retry count and maximum transaction lifetime.
- Queue entry expiry policy.
- Heartbeat interval, if heartbeat is retained.
- Button long-press threshold and debounce.

Use unsigned elapsed-time arithmetic compatible with MCU uptime counter wrap. Avoid indefinite retry loops.

## 10. Nonvolatile storage

Potential persistent values:
- Device ID/configuration if not compile-time only.
- Next request sequence/generation.
- In-progress active request identity/state if required for safe recovery.

EEPROM/flash writes have finite endurance. Do not write every heartbeat or repeated packet. Define atomic update and recovery behavior before relying on persistence. Uno and ESP32 storage mechanisms differ; hide those details behind a small platform abstraction.

## 11. Diagnostics

Provide a serial diagnostic mode for development, with:
- Firmware version and protocol version.
- Device role and configured ID.
- Network ID and RF profile.
- LoRa initialization result.
- Queue depth and active request ID.
- Packet counters, retry counters and last error.
- RSSI/SNR when available.

Serial diagnostics are for developers/maintainers; they are not a required student interaction or a substitute for LCD/buzzer feedback.
