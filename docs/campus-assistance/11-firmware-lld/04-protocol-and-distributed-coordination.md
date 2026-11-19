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


## 13. Claim withdrawal and implementation contract

CLAIM withdrawal resolves the driver-disable race while a cart is waiting for station arbitration.

| Message | Direction | Meaning |
|---|---|---|
| CLAIM_WITHDRAW | Cart → station | Withdraw this cart's unresolved claim transaction; not request cancellation |

The withdrawal carries the same full RequestId and transaction identity as the original CLAIM. The station must serialize withdrawal and assignment commit against the same authoritative request record:

- If withdrawal commits first, close that claim transaction and reject a later commit from it.
- If assignment commit wins first, return/broadcast the already committed ASSIGN; never roll it back merely because a delayed withdrawal arrived.
- Duplicate withdrawal is idempotent.
- A late withdrawal cannot erase a committed assignment.
- The cart remains NOT_ACCEPTING after disable, continues receiving and queueing, and reconciles the final station outcome.

The current typed message catalogue includes CLAIM_WITHDRAW. Numeric IDs, exact wire encoding, retry windows and arbitration airtime remain deliberately open.

The byte transport boundary, typed event delivery, atomic repository guard and SOLID ownership rules in document 06 are normative where earlier pseudocode differs.

## 14. Review corrections — withdrawal transaction and envelope retention

- CLAIM_WITHDRAW has its own payload containing the full RequestId, cart ID and original claim transaction ID. It is not encoded as a generic CLAIM payload.
- The station persists/serializes the claim lifecycle IDLE -> PENDING -> WITHDRAWN | COMMITTED. Withdrawal and assignment commit must use the same transaction identity and same authoritative record.
- ASSIGN can be emitted only after commitAssignmentIfClaimPending succeeds. A delayed withdrawal cannot reverse COMMITTED. A duplicate withdrawal is idempotent.
- A ProtocolEvent carries the complete decoded ProtocolHeader (protocol version, network, message type, source, destination, transaction ID and message sequence) plus sender-validation result. This metadata survives decoding and event dispatch for authorization, replay filtering, deduplication and ACK correlation.
- The validator must enforce role/address rules before an application event is emitted. NETWORK_MEMBER_VALIDATED means only that configured network/address rules passed; it is not cryptographic sender authentication. Authenticated status is available only after a real authentication mechanism is implemented and verified.
- Message sequence/replay windows are scoped by sender identity and protocol version; transaction IDs correlate claim/withdraw/assignment and ACK outcomes. Duplicate and stale frames must be idempotent.


## 15. Provisional host codec (not a frozen wire standard)

The current `BinaryFrameCodec` is a host-testable reference implementation only. Its
magic bytes, numeric field widths, big-endian encoding and CRC32 choice are provisional.
They must not be treated as approved interoperability or security requirements. Before
deploying radio nodes, review and freeze one protocol schema covering message IDs,
payload field layouts, maximum lengths, sequence/replay rules, ACK correlation,
arbitration windows, retry/backoff and airtime. The current application-level request
and cancellation payload assembly is likewise provisional.


## 16. Current station claim arbitration policy

The current application reference policy is first-valid-claim-to-station wins:
the station evaluates a claim and immediately attempts the repository's atomic
commit using the current station uptime. A successful commit is followed by a
station-originated ASSIGN broadcast. CLAIM_RESULT communicates rejection,
withdrawal, or committed outcome; it never independently activates the cart.

This policy deliberately has no collection/arbitration delay. It is deterministic
under the station's serialized event loop, but it does not guarantee fairness
between carts whose RF transmissions collide or arrive at different times. A
future fairness window would require an explicit timing/airtime policy and must
retain commit-time expiry and withdrawal-versus-commit atomicity.

The portable cart application now starts a claim from the oldest local FIFO entry
when the configured UI interpreter emits CONFIRM and the driver is ACCEPTING.
Journey progression emits sequenced JOURNEY_UPDATE messages. The local cart
updates its state only after the outbound message is accepted by the protocol
endpoint; station confirmation/reconciliation remains authoritative for recovery.


## 17. Current provisional application payload layouts

The application layer currently assembles the following big-endian payloads. These
layouts are implementation scaffolding, not a frozen interoperable standard:

| Message | Payload fields | Bytes |
|---|---|---:|
| REQUEST | RequestId (station u16, sequence u32, generation u16), destination u16, local age ms u32 | 14 |
| CLAIM | RequestId, cart ID u16 | 10 |
| CLAIM_WITHDRAW | RequestId, cart ID u16 | 10 |
| CLAIM_RESULT | RequestId, ErrorCode u8, ClaimState u8 | 10 |
| ASSIGN | RequestId, assigned cart u16, assignment epoch u16, RequestState u8 | 13 |
| CANCEL | RequestId, CancellationSource u8 | 9 |
| EXPIRED | RequestId | 8 |
| JOURNEY_UPDATE | RequestId, assigned cart u16, JourneyStatus u8, event sequence u32 | 15 |

The protocol endpoint still needs matching strict decoders/encoders, message-specific
length/range validation, acknowledgement correlation, duplicate replay handling and
snapshot request/response support. Transaction IDs currently derived from RequestId
hashes are also provisional; a durable collision-safe transaction allocator is
required before field deployment.


## 16. Journey update confirmation and cart release

A cart must not release its active-assignment slot merely because it queued a
COMPLETED journey update. The station remains authoritative for the lifecycle and
may not have received that update yet. On receiving a valid journey update, the
station applies it to the request lifecycle, reads back the resulting record, and
sends a station-originated SNAPSHOT to the assigned cart. The snapshot carries the
authoritative request state, assignment epoch and last journey event sequence.

The cart reconciles this snapshot through EventReconciler. A COMPLETED snapshot
allows the cart to clear its local terminal assignment and remain NOT_ACCEPTING;
the driver must explicitly re-enable acceptance. A duplicate journey update may
cause the station to resend the current snapshot. A transport ACK alone is not
sufficient evidence that the station committed the lifecycle transition.


## 17. Delivery transaction versus claim transaction

A CLAIM_WITHDRAW has two distinct identities. Its frame-envelope transaction ID
correlates delivery/retry/ACK for the withdrawal message. Its payload carries the
original claim transaction ID, which is the station repository key used to match
and withdraw the pending claim. These IDs must not be conflated: a withdrawal may
be queued while the original CLAIM is still awaiting a transport ACK.

Station request, cancellation, assignment and expiry deliveries also use distinct
transaction IDs derived from the complete RequestId and a message-phase discriminator.
A transport ACK therefore cannot accidentally acknowledge a different phase of the
same request. These encodings remain provisional until the wire schema is frozen.


## 18. Reliable broadcast fan-out and ACK ownership

A single ACK from an arbitrary cart must never complete a station broadcast. The
reference endpoint therefore expands a reliable broadcast (destination ID zero)
into one unicast retry entry per enabled cart in the provisioned station cart
registry. Each recipient gets a derived delivery transaction ID while the payload
continues to carry the same RequestId and domain data. ACK processing verifies that
the ACK source matches the unicast destination recorded by the retry entry. A
broadcast without a non-empty, validated cart roster fails closed.

The cart registry is provisioning data, not an inferred list of RF peers. A cart
must be present in the station roster to receive reliable fan-out. Roster updates
and their persistence/recovery policy still need to be integrated into target
provisioning.


## 19. Snapshot claim reconciliation fields

The provisional SNAPSHOT payload includes both lifecycle/assignment state and the
claim tuple: claim state, claimant cart ID and original claim transaction ID. This
lets a rebooting or communication-uncertain cart distinguish its own still-pending
claim from another cart's claim, a withdrawn claim, an unassigned request, or an
authoritative assignment. A cart preserves a matching in-flight claim; a mismatched
claim is resolved locally without treating CLAIM_PENDING as assignment ownership.


## 20. Cart-side semantic confirmation loop

A transport ACK for a JOURNEY_UPDATE confirms frame receipt only; it does not prove
that the station lifecycle transition was committed. After a cart queues a journey
update, it retains the expected RequestId and event sequence. Until a station
SNAPSHOT confirms that sequence (or a newer authoritative sequence), the cart
periodically sends a uniquely correlated SNAPSHOT_QUERY. Query attempts use
distinct delivery transaction IDs so bounded duplicate suppression does not hide
later application-level queries. The cart clears this synchronization obligation
only after reconciling a sufficiently new station snapshot.


## 21. Withdrawal-before-claim tombstone

CLAIM_WITHDRAW can arrive before the corresponding CLAIM if radio submission,
retransmission or receive timing reorders the two frames. When a withdrawal names
an existing unassigned RequestId but no claim is pending, the station repository
persists a WITHDRAWN tombstone containing the cart ID and original claim transaction
ID. A later CLAIM with that exact tuple is rejected as stale; another cart with a
different transaction may still claim the request. Repeated identical withdrawals
are idempotent. This tombstone is part of the durable RequestRecord snapshot.
