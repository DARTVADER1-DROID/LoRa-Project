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
- Each valid new text message is acknowledged automatically.
- Receivers suppress duplicate display while still acknowledging retries.
- Sender collects acknowledging device IDs for a bounded window and retries up to the configured limit.
- Peer tables expire after a configurable heartbeat timeout.
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

A cart's claim is provisional. It is not assigned until it receives an explicit station `ASSIGN` confirmation. A station accepts only one claim for a request. Claims carry the station ID, request ID, cart ID and assignment generation. Stale claims/assignments from an earlier generation are ignored.

A cart must not assume it owns a request merely because it transmitted a claim. Likewise, a station must not tell a student that help is assigned until it has confirmed assignment.

## Campus queue

- FIFO ordering.
- Queue capacity is configurable; initial default is 10 pending requests.
- No existing request is overwritten when full.
- A full station rejects a new request with a clear local audible/visual error.
- Each cart handles one active assignment at a time and keeps a bounded pending queue.

## Known limitations to resolve before deployment

- A station can only arbitrate claims for requests it originated; multi-station state reconciliation needs field testing.
- LoRa is half-duplex and broadcast packets/ACKs can collide. Random backoff is not a guarantee.
- A cart that disappears while physically moving may still be moving; reassignment requires operator warning and an explicit revoked-generation check.
- Timeouts need tuning against actual spreading factor, bandwidth, coding rate, airtime, campus range and regulatory duty-cycle.
