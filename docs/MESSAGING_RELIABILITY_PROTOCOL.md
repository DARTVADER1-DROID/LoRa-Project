# Reliable Messaging Protocol (RMP) v1

Status: normative target specification for component-to-component text messaging. This document defines the reliability contract; it does not claim that every feature is already implemented in firmware.

## 1. Scope and delivery promise

RMP provides bounded, best-effort reliable message delivery over a half-duplex SX1278 LoRa broadcast network for Arduino-class MCUs.

Current application constraints:
- Text payload maximum: 128 bytes (not necessarily 128 Unicode characters).
- Human-readable device names are manually configured in firmware.
- MCU-level automatic acknowledgements.
- Firmware-configurable retry and timeout limits.
- Bounded peer tables, duplicate caches, and ACK queues.
- No central coordinator for ordinary peer messaging.

**Delivery promise:** the sender can confirm that a particular known receiver MCU accepted a message. It cannot prove that every powered-off, undiscovered, hidden, or unknown device received a broadcast. An MCU ACK does not prove a human read the message.

A timeout means **not confirmed**, never **not received**. The UI MUST NOT claim "delivered to everyone".

## 2. Normative terms

- **MUST / MUST NOT:** mandatory for correctness.
- **SHOULD / SHOULD NOT:** recommended unless a documented constraint prevents it.
- **MAY:** optional.
- **Transaction:** one logical message with a stable identity.
- **Attempt:** one RF transmission of that same transaction.
- **Expected peer set:** snapshot of live known peers when the sender starts a transaction.
- **Accepted:** validated and placed into the receiver's local output path, or recognized as an already accepted duplicate.

## 3. Identity and transaction key

A message is uniquely correlated by:

`(networkId, senderDeviceId, senderBootId, messageId)`

The duplicate cache MUST use this tuple. An ACK MUST contain the original sender ID, sender boot/session ID, and message ID. The ACK sender fields identify the acknowledging receiver and its current boot/session.

Rules:
1. Device IDs MUST be nonzero and unique within a deployment. Random 16-bit IDs are only a prototype allocation strategy; collision detection/provisioning is required before larger deployment.
2. Boot ID distinguishes sequence reuse after reboot. It is a session discriminator, not authentication. A short random boot ID can collide; future versions SHOULD use a wider session epoch or wear-aware persistent counter.
3. Retries MUST retain the same transaction ID and payload. A new logical message MUST get a new ID.
4. A transaction key MUST NOT be reused while delayed packets/ACKs may remain in flight. Future versions SHOULD use a 32-bit transaction counter plus a 32-bit boot/session epoch.
5. Names are labels only, never security identities or ACK keys.

## 4. Frame validation

The current firmware uses a fixed-size packed C++ struct. This is acceptable only while both ends use the exact same layout. A future wire-format revision SHOULD use explicit byte serialization.

A versioned envelope SHOULD include: magic, protocol version, network ID, packet type, sender ID, sender boot/session ID, message ID, ACK target tuple, payload length, and payload.

Before processing, a receiver MUST validate:
- Exact supported frame length and protocol version.
- Magic, network ID, packet type, and nonzero identity fields.
- Payload length within 0–128 bytes.
- String termination/encoding rules.
- Modem CRC status where exposed by the library.

LoRa CRC detects accidental corruption; it is not authentication. RMP v1 does not provide encryption, authenticated identity, or replay protection against an attacker.

## 5. Sender state machine

`IDLE -> VALIDATE -> SNAPSHOT_PEERS -> TRANSMIT -> COLLECT_ACKS -> RETRY_WAIT -> COLLECT_ACKS -> FINAL`

Terminal outcomes:
- `CONFIRMED_TO_N`: all N peers in the expected snapshot acknowledged.
- `PARTIALLY_CONFIRMED`: one or more, but not all, expected peers acknowledged by deadline.
- `UNCONFIRMED`: no expected peer acknowledged by deadline.
- `NO_KNOWN_RECIPIENTS`: peer snapshot empty; a best-effort broadcast MAY be sent, but no delivery claim is allowed.
- `REJECTED_LOCAL`: invalid/empty/overlength input or local resource failure.
- `BUSY`: another outgoing transaction is active.

Sender algorithm:
1. Reject empty input and payloads over 128 bytes before RF transmission. Measure bytes, not displayed characters. Never silently truncate.
2. Freeze payload and transaction identity.
3. Snapshot live peers as (device ID, boot/session ID). A peer discovered later MUST NOT be silently added to this transaction.
4. If snapshot is empty, report `NO_KNOWN_RECIPIENTS`. One best-effort broadcast is permitted.
5. Transmit DATA and start the ACK collection timer.
6. Continue servicing incoming DATA, HELLO, ACK and local controls. ACK waiting MUST be non-blocking.
7. Accept an ACK only if network, original sender ID, original sender boot ID, message ID, and exact expected receiver ID + boot/session pair match.
8. Count each expected receiver once. Ignore duplicate, stale, mismatched, unexpected, or malformed ACKs.
9. Retransmit the exact same transaction within configured attempt and total-lifetime limits while peers remain unconfirmed.
10. At deadline, report confirmed and unconfirmed counts. Ignore late ACKs for completed transactions.

## 6. Receiver state machine

For each valid DATA:
1. Validate the complete envelope and payload.
2. Look up (network, sender ID, sender boot ID, message ID) in the duplicate cache.
3. If unseen, reserve cache capacity and accept/display exactly once.
4. If already seen, do not display again.
5. In both cases, schedule an ACK for the original sender/session/message tuple.
6. ACK only after acceptance or recognition of an accepted duplicate. If the output path cannot accept a new message, do not falsely ACK it; a future version MAY add BUSY/REJECT.
7. If ACK queue is full, log/counter the condition. A repeated DATA packet MUST create another ACK opportunity.

A receiver reset clears the prototype's volatile duplicate cache. An old retry may therefore display again after reboot. RMP v1 provides duplicate suppression only within the cache retention window and current boot session. Durable exactly-once display requires persistent history and is outside this prototype.

## 7. Retry, timing, and congestion control

All values MUST be configurable and bounded. Initial bench profile (not certified RF timing):

| Setting | Initial value |
|---|---:|
| Maximum retries | 3 |
| Total attempts | 4 (initial + 3 retries) |
| ACK collection timeout | 1,800 ms |
| Additional retry jitter | 100–350 ms |
| HELLO interval | 20,000 ms |
| Peer expiry | 90,000 ms |
| Known peers | 8 |
| Duplicate cache | 12 entries |
| Pending ACK queue | 4 entries |

Tune these values using measured airtime for the selected frequency, spreading factor, bandwidth, coding rate, preamble, payload size, and local duty-cycle requirements. Slower settings and longer packets require longer ACK windows.

Recommended evolution:
- Capped randomized exponential backoff rather than a single fixed retry delay.
- Maximum transaction lifetime as well as attempt count.
- Deterministic ACK response slots derived from transaction key + receiver ID, with small jitter. This reduces simultaneous ACK collisions but cannot eliminate hidden-node collisions.
- Coalesce queued ACKs for the same transaction.
- Randomize HELLO phase to avoid synchronized discovery bursts.
- Account for half-duplex operation, TX airtime and legal duty-cycle.

The LoRa library's synchronous `endPacket()` still occupies the MCU during transmission. ACK waits and scheduling MUST remain non-blocking.

## 8. Peer discovery and recipient semantics

HELLO maintains a temporary peer table. A peer is live only within the expiry window.
- Tables MUST be bounded.
- Expired entries MAY be evicted; live entries MUST NOT be silently overwritten.
- Table saturation MUST be visible in diagnostics.
- A reboot changes the peer boot/session ID.
- ACK from a rebooted peer MUST NOT satisfy an expectation for its previous session.
- The peer snapshot is a best-known roster, not proof that all network devices are known.

A newly powered-on peer may receive a broadcast but is not part of the sender's confirmed-recipient count if it was absent from the snapshot.

## 9. Queue and memory safety

- All queues/caches MUST have hard capacity limits and explicit overflow behavior.
- Full ACK queue: report overflow and rely on DATA retry for another ACK opportunity; never claim delivery.
- Full peer table: report discovery capacity; never silently evict a live peer.
- Active outgoing transaction slot: return BUSY; do not overwrite it.
- Oversized serial line: discard the entire line through newline, then recover for the next line.
- Packet buffers MUST be bounds-checked and terminated before string operations.
- Measure SRAM and stack headroom on the actual board. Successful compilation alone does not prove runtime memory safety.

## 10. Failure matrix

| Failure | Required behavior |
|---|---|
| DATA lost | Sender retries same transaction |
| ACK lost | Duplicate DATA causes another ACK |
| DATA duplicated | Display once within cache window; ACK again |
| ACK duplicated | Count receiver once |
| ACK from wrong device/session | Ignore |
| ACK for old transaction | Ignore |
| Receiver reboot | New session ID; old ACK cannot confirm current session |
| Sender reboot | New session ID; old transaction ACK cannot match |
| Peer not discovered | No all-device delivery claim |
| ACK queue full | Diagnostic; retry provides another opportunity |
| Peer table full | Diagnostic; do not silently evict live peer |
| RF collision | Bounded retry; final result partial/unconfirmed |
| Retry budget exhausted | Finalize visibly; never claim success |
| Payload >128 bytes | Reject entire input; do not send truncated prefix |
| Reset after display but before ACK | Retry may display again after volatile cache loss; document at-least-once across reboot |

## 11. Security and operational boundary

RMP v1 is not secure messaging. Before deployment beyond a controlled prototype, add authenticated enrollment, collision-resistant identities, a message authentication code, replay protection, and (if needed) reviewed encryption/key management. A network ID, device ID, boot nonce, or LoRa CRC is not cryptographic security.

## 12. Verification and release gates

Automated fault-injection tests MUST cover:
1. Lost DATA and lost ACK.
2. Duplicate DATA and duplicate ACK.
3. Wrong sender, receiver, session, and message ID.
4. Delayed ACK from a previous transaction.
5. Peer join/leave and expiry.
6. Retry exhaustion and total transaction lifetime.
7. Duplicate cache eviction and sequence wrap.
8. ACK queue and peer table overflow.
9. 128 bytes, 129 bytes, empty payload, and multi-line serial input.
10. Receiver reset before acceptance, after acceptance, and before ACK.
11. Sender reset during retry.
12. Simultaneous senders and ACK responders.

Physical validation MUST use at least three nodes and vary distance, orientation, obstruction, supply voltage and radio settings. Inject packet loss/collisions and power-cycle nodes during transactions. Record packet traces, RSSI/SNR, attempt counts, latency, duplicate displays, false confirmations, and SRAM headroom.

Release gates:
- Zero false confirmations in the test campaign.
- No unbounded wait, queue growth, or buffer overflow.
- Every transaction ends in a documented status within its maximum lifetime.
- No duplicate display within an active cache window.
- Measured performance and legal RF configuration documented.

Host tests and compilation are necessary, not substitutes for hardware validation.

## 13. Implementation status

Implemented in the current messaging sketch:
- 128-byte input limit and whole-line overflow discard.
- Versioned fixed-size RMP v1 envelope with explicit payload length and strict frame/type/identity validation.
- 32-bit persistent EEPROM boot/session epoch and 32-bit message IDs; message IDs are not reused after wrap within one boot session.
- Expected-peer snapshot with both device ID and boot/session ID.
- Automatic ACK correlated by explicit original sender ID, sender session ID and original message ID.
- Bounded non-blocking sender state machine with configurable retry count, ACK timeout, capped randomized exponential backoff and a 10-second transaction lifetime.
- Distinct final outcomes: CONFIRMED_TO_N, PARTIALLY_CONFIRMED, UNCONFIRMED and NO_KNOWN_RECIPIENTS.
- 12-entry volatile duplicate cache; duplicate DATA is not displayed twice while cached.
- Four-entry ACK queue with transaction coalescing, deterministic pseudo-slot selection and jitter; queue overflow is counted and reported.
- Peer-table saturation is counted and reported; live peers are not silently overwritten.
- Basic identity-collision warning for duplicate 16-bit device IDs.
- Local radio TX error checks and bounded packet-size assertion.

Still required for full deployment readiness:
- Collision-resistant device identity provisioning; current 16-bit device IDs remain a prototype limitation.
- More rigorous late-ACK diagnostics and deterministic collision-free ACK slot allocation.
- Cryptographic authentication/replay protection and optional reviewed encryption/key management.
- EEPROM endurance validation for the boot epoch under the expected reboot rate.
- SRAM/stack measurements and hardware-in-the-loop RF validation across at least three nodes.
- Confirmation that RF settings and airtime comply with the applicable local rules.

Keep this status section synchronized with firmware changes.
