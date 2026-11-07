# Initial application protocol

Both firmware applications use LoRa point-to-multipoint broadcasts on a configured network ID. They are separate applications and must not be assumed interoperable.

## Shared principles

- Explicit network ID and protocol magic/version fields.
- Device IDs are application identifiers, not authenticated identities.
- Message/request IDs are used for duplicate suppression.
- CRC is enabled in the LoRa modem for accidental corruption detection; it is not authentication.
- All queues, peer tables and retry counts are bounded.
- Retry and timeout settings are configurable.
- Broadcast acknowledgement traffic is randomized to reduce (not eliminate) collisions.
- Acknowledgement means the receiving MCU accepted a valid packet; it does not prove that a human saw or acted on it.

## Messaging firmware

- One common broadcast group.
- Text payload limit: 128 bytes in the initial ASCII-oriented implementation. Multi-byte UTF-8 text consumes multiple bytes; a later iteration should make that explicit in UI validation.
- Device HELLO packets maintain a local, temporary peer table.
- Each valid new text message is acknowledged automatically. ACKs correlate original sender ID, sender boot nonce, and message ID; receiver identity is independently checked against the sender's live-peer snapshot.
- Receivers suppress duplicate display using a bounded `(sender ID, sender boot nonce, message ID)` cache while still acknowledging retries. Cache eviction means duplicate suppression is a finite window, not permanent history.
- Sender snapshots expected live peers at message start, counts each peer once, retries the same transaction with bounded non-blocking timers, and reports confirmed count versus the snapshot. An empty peer snapshot gets one best-effort broadcast and is explicitly reported as `NO_KNOWN_RECIPIENTS`.
- Incoming messages continue to be serviced while the sender awaits ACKs. Receiver ACKs use a bounded non-blocking jitter queue; a full queue relies on sender retransmission to recover.
- Peer tables expire after a configurable heartbeat timeout. The boot nonce is a short random session discriminator, not cryptographic authentication.
- No encryption or message history in this prototype.

## Campus assistance firmware

Request lifecycle (station is authoritative for its own request):

`PENDING -> CLAIM_WINDOW -> ASSIGNED -> COMPLETED`

Alternative transitions:

- `PENDING -> CANCELLED_BY_STUDENT`
- `ASSIGNED -> CANCELLED_BY_STUDENT`
- `ASSIGNED -> CANCELLED_BY_CART` then return to `PENDING` for reassignment
- `PENDING -> EXPIRED`
- `ASSIGNED -> CART_OFFLINE` then return to `PENDING` for reassignment

A cart's claim is provisional. The station collects unique claimants for a bounded window and selects one deterministically using rotating cart-ID order to reduce repeated priority for the same cart. Claim storage is bounded. The selected cart is not assigned until it accepts the explicit station `ASSIGN` and returns a matching `ASSIGN_ACK`. Duplicate assignments are ACKed idempotently; stale generations and mismatched station/cart identities are rejected.

A cart must not assume it owns a request merely because it transmitted a claim. Likewise, a station must not show confirmed assignment until it has received the matching cart MCU acknowledgement.

## Campus queue

- FIFO ordering.
- Queue capacity is configurable; initial default is 10 pending requests.
- No existing request is overwritten when full.
- A full station rejects a new request with a clear local audible/visual error.
- Each cart handles one active assignment at a time and keeps a bounded pending queue.

## Known limitations to resolve before deployment

- A station can only arbitrate claims for requests it originated; multi-station state reconciliation needs field testing.
- LoRa is half-duplex and broadcast packets/ACKs can collide. Random backoff is not a guarantee.
- Student cancellation is locally terminal at the station and retried with a bounded policy. A targeted assigned-cart cancellation requires that cart's MCU ACK; a pending broadcast cancellation can only confirm the carts that actually ACK, not prove every cart cleared its queue.
- Cart-originated cancellation and completion are retried as transactions. The station validates the currently assigned cart and exact request generation, commits the transition, and ACKs. It remembers the last terminal tuple so a duplicate retry after a lost ACK is ACKed again without reapplying the transition.
- Cart request/cancellation ACK replies are scheduled with bounded non-blocking jitter. This reduces simultaneous response collisions but does not eliminate them. Acknowledgement confirms MCU packet processing only, not human awareness or physical stopping.
- Carts keep a bounded recent-cancellation tombstone cache keyed by station, request and generation to prevent delayed broadcasts from resurrecting closed requests. Tombstones are finite and can be evicted; a power cycle clears volatile tombstones.
- A cart that disappears while physically moving may still be moving. Heartbeat timeout now blocks automatic redispatch and marks the assignment uncertain. A returning heartbeat updates liveness but does not clear the safety lock; an operator must physically inspect the cart and hold the station button for five seconds to advance the generation and permit redispatch.
- Timeouts need tuning against actual spreading factor, bandwidth, coding rate, airtime, campus range and regulatory duty-cycle.


## Reliability implementation notes

- Messaging and cart reply delays use millis()-driven queues; the main loop does not block waiting for ACK windows or randomized ACK delays. The LoRa library's packet transmission call itself remains synchronous in this prototype.
- Messaging expected-recipient accounting is a snapshot. A peer joining after transmission begins is not silently added to the delivery contract.
- Cart cancellation/completion ACK retries are idempotent. The station retains a bounded four-entry terminal tuple cache in RAM; it does not survive station reboot and is not a substitute for durable transaction storage.
- Cart claim arbitration uses a bounded claimant array and a rotating cart-ID winner. More claimants than the configured bound cannot be represented in one claim window.
- These reliability additions have host-side reference-model fault-injection tests and source-contract checks. Hardware radio collision, range, timing, reboot and power-loss behavior still require physical tests.


## Component-to-component messaging reliability

The normative messaging reliability specification is maintained separately in [MESSAGING_RELIABILITY_PROTOCOL.md](MESSAGING_RELIABILITY_PROTOCOL.md). It defines the delivery contract, transaction identity, sender/receiver state machines, ACK correlation, retries, peer discovery, bounded queue behavior, failure handling, security boundary, and release acceptance tests.

The current firmware is a prototype implementation of part of that specification. See the implementation-status section in the dedicated document; CI success is not equivalent to physical RF validation.
