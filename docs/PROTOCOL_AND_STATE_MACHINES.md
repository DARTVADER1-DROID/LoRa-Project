# LoRa Protocols and State Machines

This document describes the two independent application protocols implemented in this repository. They share the SPI radio interface but use different packet formats and network IDs. They are not wire-compatible.

## 1. Shared radio interface

| Signal | Arduino Uno pin |
|---|---:|
| SCK | D13 |
| MISO | D12 |
| MOSI | D11 |
| NSS / CS | D10 |
| RESET | D9 |
| DIO0 | D2 |

Both sketches configure NSS=10, RESET=9, DIO0=2 and enable radio CRC. Current compile-time frequency is 433 MHz for bench development only. Verify module and antenna support and local rules before transmitting.

## 2. Group Messaging Protocol (RMP v1)

### 2.1 Frame layout

Packed RadioPacket is statically asserted to be 172 bytes.

| Field | Type | Meaning |
|---|---|---|
| magic | uint16 | 0x4C52 frame marker |
| version | uint8 | Currently 1 |
| networkId | uint16 | Group ID; currently 1001 |
| type | uint8 | HELLO=1, TEXT=2, ACK=3 |
| senderId | uint16 | Device identity |
| senderBootId | uint32 | Persistent boot/session epoch |
| messageId | uint32 | Sender message sequence |
| ackForSenderId | uint16 | Original message sender |
| ackForBootId | uint32 | Original sender session |
| ackForMessageId | uint32 | Original message sequence |
| payloadLength | uint8 | TEXT length; zero otherwise |
| userName | char[16] | Sender label |
| text | char[129] | Up to 128 bytes plus terminator |

Changing field order, type, packing or capacity changes the wire format. Increment protocol version and update every node together.

### 2.2 Packet types

- **HELLO:** advertises sender identity/session and name; receiver updates peer table.
- **TEXT:** carries message ID and up to 128 bytes. Duplicate suppression key is sender ID + boot ID + message ID. Duplicate TEXT is not displayed again, but is ACKed again.
- **ACK:** correlates to original sender ID, sender boot ID and message ID. It counts only if it matches the active transaction and expected peer snapshot.

### 2.3 Discovery and liveness

Peer table capacity is eight. A peer is active for 90 seconds after last reception. HELLO interval is 3 seconds when no peers are known and 20 seconds when peers are active, with up to 1 second random jitter. This is a transient estimate of recently heard devices, not a complete inventory of receivers in range.

### 2.4 Transaction flow

1. Serial input is collected until newline; CR is ignored.
2. Empty lines, NUL-containing lines and lines over 128 bytes are rejected.
3. A new message is rejected while another transaction is active.
4. Current active peer IDs and boot epochs are snapshotted.
5. Up to four total TEXT transmissions are attempted.
6. Each attempt waits up to 4200 ms for ACKs; exponential backoff starts at 150 ms, capped at 1200 ms, with jitter.
7. Transaction ends when all expected peers ACK, retry budget is exhausted, or 22 seconds elapse.
8. Sender prints confirmed, partially confirmed, unconfirmed, or no-known-recipients status.

A zero-peer send is one best-effort broadcast. It cannot provide delivery confirmation.

### 2.5 ACK scheduling

Receiver has a four-entry ACK queue. Duplicate work for the same message/target/session is coalesced. ACK delay is a 50 ms minimum turnaround plus a deterministic slot of 450 ms per peer slot and 0–50 ms jitter. Failed ACK sends are retried after 250 ms up to four attempts. Queue overflow is logged; sender retry may recover.

This is a contention-reduction heuristic, not a formal MAC protocol.

### 2.6 Reliability boundaries

- Confirmation applies only to peers in the send-time snapshot.
- Peers discovered after the snapshot are not expected recipients.
- ACK collision avoidance is heuristic.
- There is no cryptographic sender authentication or encryption.
- The 16-bit device ID must be commissioned to avoid collisions.

## 3. Campus Assistance Protocol

### 3.1 Frame layout

Packed CampusPacket is 30 bytes.

| Field | Type | Meaning |
|---|---|---|
| magic | uint16 | 0xCA51 |
| networkId | uint16 | Campus group ID; currently 2001 |
| type | uint8 | One of 14 packet types |
| senderId | uint16 | Transmitting station/cart |
| stationId | uint16 | Request origin |
| cartId | uint16 | Target/claiming cart where applicable |
| requestId | uint16 | Station-local request number |
| generation | uint8 | Request/assignment generation |
| location | char[16] | Pickup location |

There is no explicit protocol-version field. All nodes must use compatible builds. Receiver checks exact packet size, magic and network ID. Handlers perform additional checks, but this is not a cryptographic or comprehensive parser.

### 3.2 Packet type registry

| Value | Packet | Purpose |
|---:|---|---|
| 1 | REQUEST | Station broadcasts pending assistance request |
| 2 | REQUEST_ACK | Cart acknowledges receipt/queueing |
| 3 | CLAIM | Cart asks to be considered for request |
| 4 | ASSIGN | Station selects a cart |
| 5 | STUDENT_CANCEL | Station cancels request |
| 6 | CART_CANCEL | Assigned cart cancels |
| 7 | COMPLETE | Assigned cart reports completion |
| 8 | HEARTBEAT | Cart liveness and current assignment tuple |
| 9 | EXPIRE | Station closes timed-out request |
| 10 | ASSIGN_ACK | Selected cart confirms assignment |
| 11 | QUEUE_FULL | Cart cannot queue another request |
| 12 | CANCEL_ACK | Cart acknowledges station cancellation/expiry |
| 13 | CART_CANCEL_ACK | Station acknowledges cart cancellation |
| 14 | COMPLETE_ACK | Station acknowledges completion |

### 3.3 Station state flow

    IDLE
      -> short button press: create request
      -> REQUEST broadcast every 5 s
      -> collect cart CLAIM packets for 900 ms
      -> select one cart using rotating-ID preference
      -> send ASSIGN, retry every 1.5 s
      -> matching ASSIGN_ACK: assignment confirmed
      -> matching heartbeat expected every 5 s
      -> no matching heartbeat for 30 s: offline-uncertain lock
      -> physical inspection + 5 s operator hold: permit redispatch

Other station transitions:
- Long press (at least 3 seconds) while request active sends STUDENT_CANCEL.
- No assignment within 10 minutes sends EXPIRE and closes request.
- CART_CANCEL is acknowledged and returns request to pending state with a newer generation.
- COMPLETE is acknowledged and closes the request.
- Cancellation retransmits every 1.5 seconds, up to four total transmissions. If no MCU acknowledgement arrives, cancellation remains locally closed but is explicitly logged as unconfirmed.

A returning heartbeat does not clear the offline lock. This prevents radio recovery alone from being interpreted as physical recovery.

### 3.4 Cart state flow

    RECEIVE REQUEST
      -> reject tombstoned generation
      -> deduplicate existing request
      -> if queue full: send QUEUE_FULL
      -> otherwise enqueue (max 10) and send delayed REQUEST_ACK
      -> operator presses ACCEPT
      -> send CLAIM and wait
      -> matching ASSIGN: remove from queue, mark active, send ASSIGN_ACK
      -> heartbeat every 5 s while operating
      -> cancel hold or completion button: send terminal message and await ACK

A claim alone does not activate a journey; the cart requires an explicit matching ASSIGN.

### 3.5 Generations and tombstones

A request is identified by station ID, request ID and generation. The cart keeps a 12-entry tombstone cache of terminal generations and rejects delayed REQUEST/ASSIGN messages for those generations. Generation comparison uses signed modular 8-bit arithmetic. Avoid comparisons across jumps of 128 or more. Tombstones are RAM-only and disappear on cart reboot.

### 3.6 Queue and timing parameters

| Parameter | Current value |
|---|---:|
| Station request retry | 5000 ms |
| Request expiry | 600000 ms |
| Claim collection window | 900 ms |
| Cart claim retry | 2500 ms + 0–999 ms |
| Assignment retry | 1500 ms |
| Cart heartbeat | 5000 ms |
| Offline timeout | 30000 ms |
| Station cancel retry | 1500 ms; max 4 transmissions |
| Cart terminal retry | 1500 ms; max 4 transmissions |
| Cart request queue | 10 |
| Cart reply queue | 4 |
| Reply random delay | 60–649 ms |
| Tombstone cache | 12 |
| Max tracked claimants | 8 |

These are prototype starting values and require measured multi-node testing.

## 4. Protocol evolution recommendations

1. Add explicit protocol version and length to campus packets.
2. Add authenticated integrity and replay protection before trusted or safety-relevant use.
3. Add controlled device commissioning and duplicate-ID resolution.
4. Validate every field and sender/target relationship for every packet type.
5. Define request-ID and generation rollover behavior, including reset recovery.
6. Persist transaction/tombstone state if reboot recovery is required.
7. Establish airtime, retry, duty-cycle and collision budgets from measurements.
8. Keep human safety decisions independent of radio-only assumptions.
