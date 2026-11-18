# 03 — Function Contracts and State Machines

## 1. Contract rules

These are externally observable contracts, not method bodies. Implementations may add private helpers. Every failure returns an explicit error; no silent state coercion.

## 2. Station functions

| Function | Contract |
|---|---|
| StationApplication::begin(config) | Validate station role/ID/profile and initialize services; create no request |
| StationRequestController::selectDestination(id) | Accept only an enabled catalog destination |
| StationRequestController::submit() | Create unique RequestId, record station creation time and 10-minute default lifetime, queue REQUEST |
| RequestLifecycle::create(input,out) | Create one unique live request; reject duplicate identity |
| StationClaimCoordinator::evaluate(claim,now) | Reject unknown, cancelled, expired, assigned or malformed claims |
| StationClaimCoordinator::commitWinner(claim,out) | Atomically commit at most one cart for that RequestId |
| RequestLifecycle::cancel(id,source) | Apply authoritative cancellation idempotently |
| RequestLifecycle::expireDue(now,out) | Expire due unaccepted requests; never expire a committed active ride using this timer |
| RequestLifecycle::applyJourneyUpdate(update) | Accept only the assigned cart and valid sequenced status |

Creation postconditions: full identity tuple; destination is valid; station records the expiry basis; default lifetime is 600,000 ms; outbound announcement is queued but remote receipt is not claimed without evidence.

## 3. Cart functions

| Function | Contract |
|---|---|
| CartApplication::begin(config) | Initialize receiver; start NOT_ACCEPTING; do not assume an assignment |
| PendingRequestStore::insertIfAbsent(req) | Append once to FIFO; duplicate unchanged; overflow explicit |
| enableAcceptance(action) | Explicit driver action; allowed only with no active assignment or unresolved claim |
| disableAcceptance(action) | Set NOT_ACCEPTING and clear this cart's current local pending list |
| CartClaimClient::requestClaim(id,cartId) | Start one bounded claim transaction; does not imply ownership |
| onAssignmentNotice(notice) | All carts remove matching pending entry; only matching winner activates |
| ActiveAssignment::activate(record) | Require station-confirmed assignment; reject if already occupied |
| JourneyProgressController::advance(status,action) | Permit only the next valid driver-reported stage |
| ActiveAssignment::complete(evidence) | Mark terminal and force driver mode to NOT_ACCEPTING |
| EventReconciler::applyExpiryNotice(notice) | Remove only matching local pending request |

## 4. Receive pipeline

    RADIO FRAME
       -> FrameCodec.decode
       -> MessageValidator.validate
       -> ProtocolEndpoint typed event
       -> Role Application
          REQUEST: validate identity/state -> insert pending
          ASSIGN: remove pending on every cart; winning cart activates
          CANCEL / EXPIRED: remove matching pending
          SNAPSHOT: reconcile local view
          STATUS_ACK: correlate delivery transaction only

The receive pipeline runs regardless of driver mode. Driver mode gates claiming, not receiving or queueing.

## 5. Cart state machine

    NOT_ACCEPTING --driver ENABLE--> ACCEPTING_IDLE
    ACCEPTING_IDLE --oldest eligible--> CLAIMING
    CLAIMING --station rejects/timeout--> ACCEPTING_IDLE or UNCERTAIN
    CLAIMING --matching station ASSIGN--> SERVING
    ACCEPTING_IDLE --driver DISABLE--> NOT_ACCEPTING
    CLAIMING --driver DISABLE--> DISABLE/CLAIM-RESOLUTION (see open item)
    SERVING --driver confirms completion--> NOT_ACCEPTING

Invariants:
- NOT_ACCEPTING: receiver active; may queue new requests; cannot claim.
- ACCEPTING_IDLE: may claim oldest eligible request.
- CLAIMING: exactly one in-flight claim; cannot start another.
- SERVING: exactly one active assignment; cannot claim another.
- Completion always returns to NOT_ACCEPTING.
- Reboot defaults to NOT_ACCEPTING and must reconcile any possible active assignment before another claim.
- Driver disable clears the local pending list only. It does not send global cancellation.

## 6. Pending list operations

Insert preconditions: frame validated; request not known expired/cancelled/assigned. New identity appends to tail; duplicate does not reorder; full returns QUEUE_FULL; never silently evict a live item.

Disable behavior:
- Clear only the local pending store on this cart.
- Do not send REQUEST_CANCEL.
- Do not mutate station ledger or other carts.
- Keep receiving.
- Requests arriving after the clear can populate a new pending list while not accepting.

Terminal/assignment removal matches the complete RequestId. Repeated notices are idempotent. Missed notices are repaired through reconciliation.

## 7. Journey progression

    ASSIGNED / RECEIVED
          -> ON_THE_WAY
          -> ARRIVED_AT_PICKUP
          -> COMPLETED
          -> CART NOT_ACCEPTING

ARRIVED_AT_PICKUP is not COMPLETED. Status events include RequestId, assigned cart ID and monotonic event sequence. Invalid/stale transitions return INVALID_STATE/STALE_EVENT.

## 8. Station request lifecycle

    CREATED -> PENDING_UNASSIGNED -> CLAIM_PENDING -> ASSIGNED
                    |                  |
                    +-> CANCELLED      +-> resolve/retry
                    +-> EXPIRED (10 min default)

    ASSIGNED -> ON_THE_WAY -> ARRIVED_AT_PICKUP -> COMPLETED

CANCELLED and EXPIRED cannot be overwritten by delayed messages. Station atomically resolves CLAIM/CANCEL races. Pending expiry never expires an assigned journey.

## 9. Bounded service functions

    void StationApplication::service(uint32_t nowMs);
    void CartApplication::service(uint32_t nowMs);
    void ProtocolEndpoint::service(uint32_t nowMs);
    void RetryManager::service(uint32_t nowMs);
    void IButtonInput::service(uint32_t nowMs);
    void IBuzzer::service(uint32_t nowMs);

Each invocation processes bounded work and returns. No waiting for remote ACKs, button release or long delays.

## 10. Error semantics

- Radio init failure: RADIO_UNAVAILABLE; never show connected.
- Invalid frame/version: reject and count; no domain mutation.
- Queue full: preserve existing entries.
- Claim timeout: no assignment assumed.
- Missing ASSIGN: remain unresolved/uncertain until station reconciliation.
- Duplicate packet: idempotent.
- Clock rollover: wrap-safe elapsed-time arithmetic.
- Storage failure: explicit degraded/recovery state.
- Lost status ACK: bounded idempotent retry; show pending/uncertain, not physical success.
