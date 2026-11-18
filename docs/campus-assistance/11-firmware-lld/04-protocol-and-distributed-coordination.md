# 04 — Protocol and Distributed Coordination

## 1. Scope

Defines message semantics and coordination contracts, not numeric message IDs, byte offsets, field widths, RF values or cryptographic choices. Those require a separately reviewed protocol schema.

## 2. Common frame envelope

Final wire format must carry equivalent logical fields:

| Field | Purpose |
|---|---|
| protocol_version | Reject incompatible semantics/layout |
| network_id | Isolate campus networks |
| message_type | Typed command/event |
| source_device_id | Sender identity |
| destination/scope | Unicast or broadcast |
| transaction_id | Correlate ACK/retry |
| message_sequence | Duplicate/stale detection |
| payload_length | Bounded frame validation |
| payload | Message-specific fields |
| integrity_check | Accidental corruption detection |

Do not broadcast student names or sensitive personal data. LoRa CRC is not authentication.

## 3. Request identity

RequestId = (origin_station_id, station_sequence, generation/session).

Every request-related message carries the complete tuple. Sequence alone is insufficient because it may wrap or repeat after reboot. Generation persistence and rollover policy must be frozen with the storage design.

## 4. Message catalogue

| Message | Direction | Meaning |
|---|---|---|
| REQUEST | Station → broadcast | New live request |
| REQUEST_SEEN_ACK (optional) | Cart → station | Frame processed, not assignment |
| CLAIM | Cart → station | Request ownership attempt |
| CLAIM_RESULT | Station → claimant | Rejected/resolved/considered; not ownership unless explicit ASSIGN |
| ASSIGN | Station → broadcast | Committed winning cart and assignment epoch |
| ASSIGN_ACK | Winner → station | Winning cart processed ASSIGN |
| ASSIGNMENT_QUERY | Cart → station | Resolve uncertain claim/reboot |
| REQUEST_CANCEL | Station → broadcast | Student cancellation |
| REQUEST_EXPIRED | Station → broadcast | Station-authoritative expiry |
| JOURNEY_STATUS | Assigned cart → station | Driver-entered journey stage |
| STATUS_ACK | Station → cart | Protocol accepted status, not physical truth |
| STATE_QUERY / SNAPSHOT | Node ↔ station | Repair local request view |
| HEARTBEAT (optional) | Node → broadcast | Diagnostics only |

Optional messages require airtime justification before implementation.

## 5. Claim and assignment transaction

Cart preconditions:
- Driver mode ACCEPTING_IDLE.
- No active assignment or other claim.
- Candidate is oldest eligible pending request.
- Candidate is not known cancelled, expired or assigned.

Transaction:
1. Cart sends CLAIM(full RequestId, cartId, transactionId).
2. Station checks existence, expiry, cancellation and current assignment.
3. Station serializes competing claims and commits at most one winner.
4. Station broadcasts ASSIGN(full RequestId, winnerCartId, assignmentEpoch).
5. Only the matching cart activates after validating station-originated ASSIGN.
6. Every cart removes that RequestId from pending on ASSIGN.
7. Winner sends idempotent ASSIGN_ACK.
8. Station repeats/reconciles within a bounded policy until delivery is established or uncertainty is reported.

A CLAIM transmission never makes the cart the winner. A non-final CLAIM_RESULT is not assignment confirmation.

## 6. Contention and failure properties

Required:
- At most one committed winner per RequestId.
- Duplicate claim by same cart is idempotent.
- Late claim after assignment returns existing winner.
- Claims after cancellation/expiry are rejected.
- Lost ASSIGN cannot create a second winner.
- Ambiguous station state leaves carts unassigned/uncertain.

Backoff/slot allocation, arbitration window and retry count remain open. Choose using airtime, node count, collision and legal duty-cycle measurements.

## 7. Pending-list convergence

Broadcast loss can leave stale local copies. Assignment/cancel/expiry notices must be retransmitted or recoverable through query/snapshot.

Reconciliation contract:
- Cart queries authoritative state for a RequestId or bounded batch.
- Station responds with current state and owner/epoch when assigned.
- Reconciliation is idempotent.
- It removes stale assigned/cancelled/expired entries.
- It never resurrects terminal requests.
- Station ledger remains authoritative; cart list is a local cache.

Snapshot schema, cadence and airtime budget remain to be designed.

## 8. Expiry

- Default lifetime: 600,000 ms from station creation.
- Firmware configuration key: REQUEST_LIFETIME_MS or equivalent.
- Station monotonic elapsed-time basis is authoritative.
- Station rejects claims received after expiry.
- Station broadcasts REQUEST_EXPIRED with full RequestId.
- Carts remove on notice or reconciliation.
- Local cart aging is advisory memory cleanup only.
- Expiry applies only before assignment; assigned journey follows separate incident/cancellation rules.

## 9. Cancellation race

Student cancellation is station-originated. Clearing a cart's local pending list is not cancellation.

Station must atomically order simultaneous CANCEL and CLAIM:
- CANCEL commits first: later claim rejected.
- ASSIGN commits first: cancellation follows assigned-journey cancellation policy; it cannot silently erase active assignment.
- Outcomes carry full RequestId and authoritative state.
- Delayed/duplicate packets cannot reverse committed state.

Driver notification policy for cancellation after assignment remains open.

## 10. Journey status

Allowed progression: ASSIGNED/RECEIVED → ON_THE_WAY → ARRIVED_AT_PICKUP → COMPLETED.

JOURNEY_STATUS includes full RequestId, assigned cart ID, status, event sequence and transaction identity. Station accepts only the assigned cart and valid next status. STATUS_ACK proves protocol processing only.

## 11. Retry interface

    struct RetryPolicy {
      uint8_t maximumAttempts;
      uint32_t initialTimeoutMs;
      uint32_t maximumTimeoutMs;
      BackoffMode backoffMode;
    };

    class RetryManager {
    public:
      Status schedule(const Transmission&, uint32_t nowMs);
      void service(uint32_t nowMs);
      void onAcknowledgement(const Acknowledgement&);
      RetryOutcome outcome(TransactionId) const;
    };

Values must be set after airtime calculation and tests. Retries are bounded and idempotent.

## 12. Security boundary

CRC/integrity detects accidental corruption, not sender authenticity. Raw LoRa is observable and spoofable without additional controls. Define authentication, replay protection and key provisioning before real-student deployment. No personal data in packets; no security certification is implied.
