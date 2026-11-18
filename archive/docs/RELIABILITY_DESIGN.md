# Reliability protocol design

Status: design baseline for the next firmware reliability milestone. This document describes intended behavior and identifies gaps between the current prototype and the target. It does not claim that the algorithms below are already implemented.

## 1. Design goals and invariants

Both applications run on small AVR MCUs with bounded RAM, half-duplex LoRa radios, and no always-on coordinator beyond the campus station. Reliability therefore means bounded retries, explicit uncertainty, idempotent state transitions, and recovery from packet loss. It does not mean guaranteed delivery.

Shared invariants:

1. A packet is accepted only after exact-length, magic, network, type, and integrity validation.
2. A retransmission repeats the same logical transaction identifier and payload; it must not create a second operation.
3. Every state-changing command is idempotent.
4. A receiver acknowledges only after it has validated and applied (or recognized as already applied) the operation.
5. An ACK identifies the original sender and transaction, not merely a small sequence number.
6. All tables, queues, retry counts, and waiting periods are bounded.
7. No blocking wait or long radio-response delay may prevent the MCU from servicing buttons, incoming packets, or safety-relevant state.
8. A timeout means "not confirmed"; it must never be presented as proof that the remote device did not act.
9. Device IDs are not authentication. CRC detects accidental corruption but does not prevent spoofing or replay by an attacker.
10. The firmware must fail visibly and safely when queue capacity, identity uniqueness, or protocol state cannot be established.

## 2. Shared transport envelope (next protocol version)

The current sketches use fixed-size packed structs. Keep the wire format fixed-size for the AVR prototype, but introduce an explicitly versioned envelope before adding more message types.

Recommended logical fields:

| Field | Purpose |
|---|---|
| magic + protocolVersion | Reject unrelated or incompatible frames |
| networkId | Separate deployments |
| packetType | HELLO, DATA, ACK, REQUEST, CLAIM, ASSIGN, CANCEL, etc. |
| senderId + destinationId | Identify source and optional unicast target |
| senderBootId | Distinguish sequence reuse after reboot |
| transactionId | Stable ID for the logical operation |
| generation | Fence stale assignment/cancellation operations |
| payloadLength | Validate the meaningful payload bytes |
| payload | Bounded application data |
| modem CRC | Detect accidental RF corruption |

Use explicit serialization rather than relying indefinitely on compiler-specific packed-struct layout. During migration, both ends must use the same protocol version; reject unknown versions instead of interpreting them.

A 16-bit message counter alone is not globally unique. Pair it with sender ID and a boot/session identifier, or persist a monotonically increasing counter with wear-aware storage. Do not write EEPROM for every text message. A boot identifier may be generated at startup, but it must be checked for zero and collisions where practical.

## 3. Requirement A — component-to-component text messaging

### 3.1 Delivery contract

A text message is one logical transaction, at most 128 payload bytes in the current ASCII-oriented implementation. The byte limit is authoritative: UTF-8 characters may occupy multiple bytes. The sender must reject an overlength message before transmission and must never silently truncate it.

Distinguish these outcomes in the UI/serial status:

- SENT_UNCONFIRMED: radio transmission attempted; no receiver ACK observed.
- PARTIALLY_CONFIRMED: one or more known peers ACKed, but not every peer in the sender's current expected-peer snapshot.
- CONFIRMED_TO_N: N distinct receiver MCUs accepted the message.
- NO_KNOWN_RECIPIENTS: no live peers were known when the message was sent.
- EXPIRED: bounded retry budget elapsed; do not imply that no receiver got the message.

An MCU ACK means the receiver validated and accepted the message into its local display/output path. It does not mean a human read it. The application has no reliable way to know every powered-off or undiscovered device on a broadcast network. Therefore, "delivered to everyone" must not be claimed.

### 3.2 Sender state machine

IDLE -> TX_PENDING -> WAIT_ACKS -> RETRY_WAIT -> WAIT_ACKS ... -> COMPLETE | PARTIAL | UNCONFIRMED

Algorithm:

1. Validate non-empty input and lengthBytes <= 128.
2. Allocate one transaction ID and freeze the payload. Never change the ID or payload during retries.
3. Snapshot the currently live peer IDs as the expected set. If the set is empty, report NO_KNOWN_RECIPIENTS; optionally still transmit once as best effort, but do not call it confirmed.
4. Transmit once, then continue servicing RX and local input while a non-blocking ACK deadline is active.
5. Accept an ACK only if network, version, ACK type, original sender ID, transaction ID, and (in the next protocol version) sender boot/session ID all match.
6. Count each receiver at most once. Ignore duplicate ACKs.
7. When all peers in the snapshot have ACKed, finish early. Otherwise retry the same transaction with bounded backoff until the configured attempt/deadline limit.
8. At deadline, report the distinct confirmed receiver IDs and the number of expected peers still unconfirmed. Keep late ACK handling bounded and associate it only with the still-valid transaction.

Recommended retry schedule: initial send plus a small configurable number of retries, with capped randomized exponential backoff. The current fixed 1.8-second ACK wait and retry loop are starting values only; tune them from measured airtime and network size.

### 3.3 Receiver state machine

For a valid new DATA transaction:

1. Validate sender, payload length, and terminator/encoding constraints.
2. Check duplicate cache using (senderId, senderBootId, transactionId).
3. If new, reserve a cache entry and emit/display exactly once.
4. Send an ACK after successful acceptance.
5. If duplicate, do not display again, but ACK again. This is essential because the original ACK may have been lost.
6. If the output path cannot accept the message, do not ACK it as accepted; return a bounded BUSY/REJECT response in a future protocol version.

The duplicate cache must have a documented retention window and capacity. A 12-entry ring is a small prototype safeguard, not a guarantee against delayed duplicates after eviction. Cache expiry and sequence-wrap behavior must be explicit.

### 3.4 Broadcast ACK collision control

Random delay reduces collisions but cannot eliminate them. For a larger group, use deterministic response slots derived from a stable hash of (senderId, transactionId, receiverId) plus a small jitter. The sender listens for the entire ACK collection window. If multiple receivers collide, missing receivers retry the ACK after a later request/retry cycle.

A later scalability step may use HELLO-discovered peer count to assign slots. Do not assume the peer table is complete: stale, hidden, or newly powered-on nodes may exist.

### 3.5 Messaging-specific failure handling

- Lost DATA: sender retries the same transaction.
- Lost ACK: receiver sees duplicate DATA and re-ACKs without redisplaying.
- Duplicate DATA: one display, multiple ACK opportunities.
- ACK from another sender or for another transaction: ignore.
- Delayed ACK after transaction expiry: ignore or log as late; never attach it to a reused ID.
- Device reboot: boot/session ID prevents old ACKs from matching new transactions.
- Peer table full: evict only expired entries; report discovery capacity limits.
- Sender receives DATA while waiting for ACKs: process it; never drain and discard non-ACK packets.
- Long RF collision burst: bounded retries expire visibly; do not block the entire UI indefinitely.
- 129-byte payload: reject before any RF transmission.
- UTF-8 input: validate byte count; either support valid UTF-8 explicitly or document ASCII-only operation.

## 4. Requirement B — campus assistance dispatch

### 4.1 Authority and lifecycle

The station is authoritative for each request. A cart's claim is provisional; a cart may begin an assignment only after receiving an explicit station grant addressed to that cart.

Logical lifecycle:

IDLE -> PENDING -> CLAIM_COLLECTION -> ASSIGNING -> ASSIGNED -> COMPLETED

Terminal/exception transitions:

- PENDING or CLAIM_COLLECTION -> CANCELLED
- ASSIGNING or ASSIGNED -> CANCELLED
- PENDING or CLAIM_COLLECTION -> EXPIRED
- ASSIGNED -> CART_CANCELLED -> PENDING with a strictly newer generation
- ASSIGNED -> CART_OFFLINE -> RECOVERY_PENDING with a strictly newer generation and operator-visible warning
- Any state -> FAULT/UNCERTAIN if identity or persistence invariants fail

Each request is identified by (stationId, requestId). Each assignment attempt is fenced by generation. A generation is compared using modular sequence arithmetic; the system must not compare an 8-bit generation with ordinary greater-than because it wraps. Do not allow two live generations to be separated by 128 or more increments.

### 4.2 Request creation and replay

1. Station allocates a request ID that cannot collide with a still-live request after reboot.
2. Station broadcasts REQUEST periodically using the same (stationId, requestId, generation).
3. Cart validates and deduplicates the request before adding it to its bounded FIFO.
4. Duplicate REQUEST for the same generation must not create another queue entry; cart repeats REQUEST_ACK.
5. A request already tombstoned at an equal or older generation is rejected and must not receive a positive queue-acceptance ACK.
6. If the cart queue is full, it sends QUEUE_FULL and does not ACK as accepted.
7. Queue overflow never overwrites an existing request.

The station should persist its next request ID in a wear-aware manner or combine a persistent boot epoch with a counter. A plain EEPROM write at every request start needs a wear analysis before deployment.

### 4.3 Fair claim collection and single-winner arbitration

The current "first claim received becomes candidate" behavior is simple but vulnerable to RF timing and hidden-node effects. The target algorithm:

1. Cart user requests the next queued job. Cart sends CLAIM with the exact request tuple and its own ID.
2. Cart retries CLAIM with bounded randomized backoff while the request remains queued and no grant/rejection arrives.
3. Station opens a fixed claim-collection window after the first valid claim and records a bounded set of unique claimants.
4. Station selects exactly one claimant deterministically (for example, configured cart priority followed by cart ID as a tie-breaker). If no priority policy is required, lowest cart ID is deterministic but can starve higher IDs; add rotating fairness if repeated ties are observed.
5. Station sends ASSIGN only to the selected cart. Other claimants receive BUSY/NOT_SELECTED or learn through the next request state.
6. Station remains in ASSIGNING until matching ASSIGN_ACK arrives. It repeats the same ASSIGN transaction within a bounded retry/deadline policy.
7. Cart accepts ASSIGN only if station/request/generation match a queued request, destination cart ID matches itself, and no conflicting active assignment exists. Duplicate ASSIGN for the same active tuple is ACKed idempotently.
8. Only after accepting ASSIGN does the cart move the request from queue to active assignment.
9. If the grant deadline expires, station marks the attempt uncertain, advances generation, and reopens dispatch. The old generation is fenced; a delayed old ASSIGN must not activate.

A station must never announce "cart assigned" to the student merely because it transmitted ASSIGN. It may show "assigning" until the cart ACK arrives.

### 4.4 Cancellation protocol and acknowledgement semantics

Cancellation is a transaction, not a one-shot broadcast.

- Student cancellation: station marks the request locally terminal immediately, then broadcasts CANCEL with exact station/request/generation. The station retries a bounded number of times.
- Assigned request: target the known assigned cart ID. A matching CANCEL_ACK from that cart confirms that its MCU processed the cancellation.
- Pending broadcast request: the station may receive ACKs from carts that heard the cancellation, but it cannot know every cart that ever queued the request unless it maintains a reliable roster. Report CANCELLED_LOCALLY; N CARTS CONFIRMED rather than "all carts cleared". Continue bounded broadcasts for the full cancellation window or until the known expected roster has acknowledged.
- Cart-side cancellation: cart sends CART_CANCEL and remains in a local CANCEL_PENDING state until station ACKs or the retry deadline expires. Station validates that the sender is the currently assigned cart and the tuple/generation match; then it returns the request to PENDING with a newer generation and ACKs the cart.
- Completion: cart sends COMPLETE; station validates the assigned cart and exact generation, commits COMPLETED, then sends COMPLETE_ACK. Cart should retain a short-lived completed tombstone and retry until ACK/deadline.
- Expiry: station broadcasts EXPIRE as a terminal operation. Carts tombstone the tuple and may send terminal ACKs.
- All ACKs include the original station/request/generation and the responding cart ID. A mismatched or stale ACK has no effect.

An ACK proves MCU processing only. It does not prove that a cart is physically stopped, that a human saw the screen, or that the cart is safe to reassign. Reassignment after cart loss/cancellation requires an operator-visible warning and a physical safety procedure.

### 4.5 Tombstones and stale-message fencing

Cart tombstone key: (stationId, requestId) with highest terminal generation observed.

- On CANCEL, EXPIRE, CART_CANCEL acknowledgement, or COMPLETE, record the highest generation.
- Reject any REQUEST or ASSIGN at an equal or older generation.
- Accept a strictly newer generation only when it is valid under modular comparison.
- Tombstone cache is bounded. Eviction policy must prefer expired/oldest entries, not blindly overwrite a potentially active tombstone.
- Tombstones in RAM are lost on power failure. Before deployment, either persist compact terminal records or require a station/cart resynchronization handshake after reboot.
- On startup, a cart must not accept a replayed request until it has completed a station synchronization step or the protocol explicitly accepts the risk of volatile state loss.

### 4.6 Liveness, heartbeats, and offline recovery

- Cart heartbeat includes cart ID and active assignment tuple; use a heartbeat sequence/boot ID in the next protocol version.
- Station refreshes the lease only for the currently assigned cart and exact generation.
- A missing heartbeat does not prove the cart is stopped. Mark the assignment CART_OFFLINE/UNCERTAIN, alert the operator, and do not silently treat the physical cart as available.
- Reassignment is allowed only after an explicit policy/physical check and a generation increment that fences the old assignment.
- Cart periodically retries its active-state heartbeat and station periodically repeats state confirmation.
- A rebooted station/cart performs reconciliation before accepting new work.

## 5. Radio scheduling and MCU constraints

The SX1278 link is half-duplex. Every ACK and retry consumes airtime and can collide with other nodes.

- Avoid delay() in receive/ACK paths. Use millis()-driven state machines for ACK slots, retry deadlines, button debounce, and UI refresh.
- Never perform long blocking ACK collection loops that discard other valid packet types.
- Use a small fixed packet queue/ring buffer; define behavior when full.
- Compute airtime using the actual spreading factor, bandwidth, coding rate, preamble, and payload length. Configure response windows from measured worst-case airtime, not arbitrary constants.
- Add a randomized channel-access delay before unsolicited broadcasts and ACKs.
- Keep packet sizes within the LoRa modem maximum and the selected regional duty-cycle / dwell-time limits.
- Avoid dynamic allocation and unbounded strings. Use fixed-size arrays and explicit lengths.
- Ensure every timer comparison uses unsigned subtraction (millis() - start >= interval) for rollover safety.
- Keep critical state transitions short; do not update LCD or print large serial logs inside timing-sensitive radio handlers.

## 6. Test and validation plan

### Host-side deterministic protocol tests

Build pure C++ state-machine helpers or a Python reference model that can inject packet events and advance a fake clock. Test at minimum:

**Messaging**
- New message, duplicate message, duplicate ACK, wrong ACK sender, wrong transaction ID, wrong boot ID.
- Lost DATA, lost ACK, late ACK, reordered ACKs, ACK collision, retry exhaustion.
- Peer table empty/full/expired; receiver joins/leaves during a send.
- Message ID wrap and reboot/session change.
- Empty, 1-byte, 128-byte, 129-byte, malformed and multi-byte UTF-8 payloads.
- Incoming DATA while sender is waiting for ACKs; prove it is processed and ACKed.
- Verify a duplicate is displayed once but ACKed on every valid retry.

**Campus**
- Duplicate REQUEST and REQUEST_ACK; queue full; FIFO preservation.
- Two or more simultaneous CLAIMs; deterministic single winner; duplicate CLAIM.
- Lost ASSIGN, lost ASSIGN_ACK, duplicate ASSIGN, wrong cart ID.
- Student cancellation before claim, during claim collection, while assigning, and after assignment.
- Lost cancellation packet, lost cancellation ACK, repeated cancellation, multiple carts acknowledging broadcast cancellation.
- Cart cancel and COMPLETE with lost station ACK and retransmission.
- Stale REQUEST/ASSIGN/CANCEL/ACK from older generations; generation wrap boundary.
- Cart reboot with volatile tombstones lost; station reboot with request counter recovery.
- Cart heartbeat loss, delayed heartbeat, old-generation heartbeat, and recovery policy.
- Queue capacity boundary, button bounce/hold, millis rollover, and packet queue overflow.

### Hardware-in-the-loop and field tests

1. Two radios at short range, then representative campus range.
2. Three or more carts claiming one request simultaneously.
3. Deliberately power off a cart during every lifecycle state.
4. Attenuate or interrupt RF to force loss, retry, and recovery.
5. Run long-duration soak tests to exercise ID wrap, timer rollover, queue churn, and EEPROM endurance.
6. Measure packet airtime, collision rate, ACK latency, retry rate, and delivery coverage.
7. Verify all deployed frequency, power, antenna, and duty-cycle settings comply with local regulations.
8. Validate the workflow with stationary carts and no drive/brake control connected.

## 7. Implementation order

1. Refactor both sketches to non-blocking receive/transmit state machines; ensure no valid incoming packet is discarded during ACK waits.
2. Add host-testable pure protocol helpers and deterministic fake-radio tests.
3. Strengthen messaging transaction identity, expected-peer accounting, duplicate-cache retention, and honest delivery status.
4. Strengthen campus claim collection, deterministic arbitration, and ASSIGN_ACK-gated status.
5. Add bidirectional acknowledged CART_CANCEL and COMPLETE transactions.
6. Correct cancellation ACK accounting for assigned versus broadcast-pending requests.
7. Add reboot reconciliation and durable identity/tombstone strategy.
8. Run host tests, all Arduino compile targets, then hardware-in-the-loop and field tests.

No firmware should be considered production-ready until the hardware and field tests pass. Do not merge or deploy based on compilation alone.


## 8. Implementation status (prototype branch)

Implemented in the current feature branch:

- Messaging ACK collection and retry deadlines are serviced from the main loop rather than a blocking ACK-wait loop.
- Messaging snapshots the known live peer IDs at send start; ACKs must match sender ID, sender boot nonce, message ID, and expected peer membership.
- Message duplicate keys include sender ID, sender boot nonce, and message ID. ACK retries do not redisplay a cached message.
- Receiver ACKs are placed in a fixed-size jitter queue; queue overflow is visible and recoverable by sender retry.
- Empty known-peer roster performs one best-effort send and reports that no recipients were known.
- Campus station collects a bounded set of distinct claims and rotates the cart-ID selection after each winner.
- Cart claim retries include bounded random jitter.
- Cart cancellation and completion retry the same transaction; the station validates the currently assigned cart and caches four terminal tuples so a lost ACK can be recovered by duplicate retry.
- Cart request and cancellation ACK replies use a bounded non-blocking jitter queue.
- A cart heartbeat timeout no longer triggers automatic redispatch. The station marks the assignment uncertain; even a returning heartbeat does not clear the safety lock. Physical inspection and five-second operator confirmation are required before generation advance and redispatch.
- Stale request generations are rejected rather than ACKed as accepted.

Still outstanding before production consideration:

- Durable station transaction identity and reboot reconciliation; current request IDs and terminal ledger have volatile/persistence limitations.
- A durable or synchronized tombstone strategy; current cart tombstones are finite and volatile.
- Authentication and replay protection; IDs, boot nonce and CRC are not cryptographic protections.
- Full fair arbitration across more than the bounded claimant capacity, hidden-node effects, and measured RF airtime/duty-cycle scheduling.
- Physical hardware-in-the-loop tests with packet loss, multiple carts, power cycling, range and interference.
- Validate the five-second operator recovery procedure with the actual station button and visible/audible feedback.
- The LoRa library's endPacket() transmission call remains synchronous; non-blocking changes remove long ACK waits and randomized response delays, not radio airtime itself.

The new Python test suite is a deterministic reference-model/fault-injection suite plus source-contract assertions. It is not a cycle-accurate emulator of the AVR firmware or SX1278 channel.
