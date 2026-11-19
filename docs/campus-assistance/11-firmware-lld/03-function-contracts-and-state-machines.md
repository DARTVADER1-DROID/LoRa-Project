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
| EventReconciler::applyAssignmentNotice(notice) | Reconcile the validated station notice; all carts remove the matching pending item and only the named winner activates |
| ActiveAssignment::activate(record) | Require station-confirmed assignment; reject if already occupied |
| JourneyProgressController::validateTransition(current,next,action) | Validate the next driver-reported stage without owning journey state |
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

Acceptance and cart service state are independent dimensions:

    AcceptanceMode: NOT_ACCEPTING <-> ACCEPTING
    CartServiceState: IDLE -> CLAIM_PENDING -> SERVING
                                  |             |
                                  v             v
                              UNCERTAIN      IDLE (after terminal clear)

- Boot starts NOT_ACCEPTING. Service state starts IDLE unless reboot reconciliation identifies a possible active assignment.
- Driver ENABLE changes acceptance to ACCEPTING only when no active assignment and no unresolved claim exists.
- Driver DISABLE immediately changes acceptance to NOT_ACCEPTING and clears only the local pending FIFO.
- If a claim is unresolved, DISABLE also sends CLAIM_WITHDRAW with the exact RequestId, cart ID and transaction ID. The cart retains the transaction until station resolution.
- If withdrawal wins, the claim closes and service state returns to IDLE. If assignment commit wins, the cart processes the station ASSIGN, retains the assignment and raises a driver-facing conflict; it does not silently discard ownership.
- SERVING means exactly one station-confirmed active assignment. It is independent of acceptance intent; a cart can be NOT_ACCEPTING while still serving its already committed assignment.
- Completion clears the active assignment and forces NOT_ACCEPTING. Manual re-enable is required.
- The receiver remains active and may repopulate the local pending FIFO while NOT_ACCEPTING.

The station serializes claim start, withdrawal, assignment commit, cancellation and expiry against the same authoritative request record. A delayed withdrawal cannot reverse a committed assignment.

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


## 11. Resolved CLAIMING disable transition

The former open transition is resolved by the normative policy in [06 — SOLID Boundaries, Ownership and Scalability Contracts](06-solid-and-scalability-contracts.md):

- Driver disable immediately makes the cart NOT_ACCEPTING and clears only its local pending FIFO.
- If a claim is unresolved, send CLAIM_WITHDRAW for that transaction and keep tracking it until station resolution.
- The station atomically orders CLAIM_WITHDRAW against assignment commit.
- If withdrawal wins, no assignment is created. If ASSIGN already committed, it remains authoritative; the cart must process it and alert the driver rather than silently discard it.
- New requests continue to be received and may repopulate the local queue while disabled.

## 12. Authoritative implementation signatures

The public header contracts supersede older shorthand signatures in this document:

- `StationApplication::service()` and `CartApplication::service()` are bounded role service calls; they obtain time through injected IClock.
- `RequestLifecycle::create(const NewRequest&, RequestRecord&)`.
- `RequestLifecycle::cancel(const RequestId&, CancellationSource)`.
- `RequestLifecycle::expireDue(uint32_t stationNowMs, ExpiredRequestSet&)`.
- `RequestLifecycle::applyJourneyUpdate(const JourneyUpdate&)`.
- `ActiveAssignment::complete(const DriverAction&)`.
- `JourneyProgressController::validateTransition(current, next, const DriverAction&)` is stateless policy; ActiveAssignment owns journey state.
- `ProtocolEndpoint::enqueue(const ProtocolMessage&, DeliveryClass)`.
- `ProtocolEndpoint::pollEvent(ProtocolEvent&)`.

No API may imply that a CLAIM, ACK or local UI action commits assignment or proves physical completion.

## 10. Review corrections — atomic race and state ownership

This section supersedes conflicting earlier shorthand in this document.

### Claim transaction lifecycle

A claim is a durable transaction, not merely a cart UI mode:

    IDLE -> PENDING -> WITHDRAWN
                   -> COMMITTED

- beginClaimIfEligible atomically records the cart and transaction ID only if the request is live, unassigned and not already claimed by another unresolved transaction.
- withdrawClaimIfPending changes only the matching RequestId/cartId/transactionId from PENDING to WITHDRAWN.
- commitAssignmentIfClaimPending commits only that exact still-PENDING transaction. It atomically verifies the request remains live and unassigned, then stores assigned cart and assignment epoch.
- The station serializes withdrawal, expiry, cancellation and assignment commit against the same record. Exactly one terminal/assignment transition wins.
- Duplicate withdrawal is idempotent. A delayed withdrawal after COMMITTED returns the committed outcome and cannot clear the assignment.
- Reboot recovery must restore claim transaction state from durable station storage or safely reject/reconcile unresolved transactions before accepting new claims.

### Independent cart state dimensions

- AcceptanceMode is NOT_ACCEPTING or ACCEPTING.
- CartServiceState is IDLE, CLAIM_PENDING, SERVING or UNCERTAIN.
- Disabling acceptance changes only acceptance intent and clears only the local pending FIFO. It sends CLAIM_WITHDRAW if a claim is unresolved; it never sends global request cancellation.
- If assignment committed before withdrawal, process the authoritative ASSIGN and surface the driver conflict. Do not silently discard it.
- Receiver operation and queue insertion continue while NOT_ACCEPTING.

### Single ownership of journey state

- ActiveAssignment owns the active assignment and current journey status.
- JourneyProgressController validates a proposed transition and is stateless.
- CartClaimClient tracks only the claim transaction.
- EventReconciler is a stateless orchestration boundary; it delegates assignment activation and journey changes to the owning domain components.
- No second current() journey-state cache is permitted.

### Repository atomicity

There is no generic repository update. Cancellation, expiry, claim start, withdrawal, assignment commit and journey update are conditional atomic transitions. A repository implementation must make each operation linearizable with respect to every other transition for the same full RequestId.

### 10.1 Commit-time expiry check

StationClaimCoordinator::commitWinner(claim, stationNowMs, assignment) must pass the current station clock value to commitAssignmentIfClaimPending. evaluate() is advisory only; the repository rechecks expiry and transaction state atomically at commit time.
