# 06 — SOLID Boundaries, Ownership and Scalability Contracts

## 1. Purpose and conformance

This document is normative for the interface-only firmware scaffold. It resolves the high-priority architectural gaps found during review of the first scaffold. It defines responsibilities and invariants; it does not claim that runtime behavior, hardware integration, radio reliability or regulatory compliance has been implemented or validated.

The shared headers under `firmware/campus_assistance/shared/include/ca/` are the machine-readable contract surface. A public contract change must update this document, the relevant class catalogue, protocol vectors and host/target compile checks in the same change.

## 2. Dependency direction

The dependency graph is strictly inward:

```text
UNO / ESP32 composition root
        |
        v
StationApplication / CartApplication
        |
        +----> role use cases (StationRequestController, claim workflow)
        |
        +----> domain abstractions (RequestLifecycle, queues, assignment, state)
        |
        +----> protocol abstraction (ProtocolEndpoint, typed ProtocolEvent)
                         |
                         v
             FrameCodec / MessageValidator / RetryManager
                         |
                         v
             IRadioTransport + other hardware ports
                         |
                         v
                 board-specific adapters
```

- Domain contracts depend on portable value types and repository/clock abstractions only.
- Protocol contracts depend on portable protocol/domain values, never UI, GPIO, Arduino or ESP-IDF.
- Applications depend on abstractions and receive collaborators through explicit dependency structs.
- Hardware adapters implement ports; they do not own request policy.
- UI translates button events into intents and renders view models. It cannot commit assignments or mutate station truth.
- Platform-specific headers are confined to target adapter translation units.

## 3. Single responsibility and ownership

| Component | Sole responsibility | Explicitly excluded |
|---|---|---|
| `ConfigValidator` | Validate provisioned board/device/network/radio settings and registries | Request submission or runtime enrollment |
| `StationRequestController` | Select destination; submit/cancel the current station request use case | Persistence internals, claim arbitration, frame encoding |
| `RequestLifecycle` | Create, cancel, expire and apply sequenced journey updates | Choosing the winning cart |
| `IRequestRepository` | Store complete authoritative `RequestRecord` values through conditional atomic transitions | UI, RF policy, driver state |
| `StationClaimCoordinator` | Evaluate claims and commit the single winning assignment | Request creation, driver input |
| `PendingRequestStore` | Bounded cart-local FIFO and exact identity removal | Global ownership or network cancellation |
| `ActiveAssignment` | Hold zero or one station-confirmed assignment | Pending FIFO or claim arbitration |
| `DriverAvailabilityController` | Enforce driver acceptance state transitions | Radio receive or request queueing |
| `CartClaimClient` | Track one bounded outstanding claim transaction | Declaring itself assigned |
| `EventReconciler` | Apply authoritative notices/snapshots idempotently | Frame parsing or request creation |
| `FrameCodec` | Encode/decode bounded wire frames and integrity field | Business validation or arbitration |
| `MessageValidator` | Validate decoded protocol envelope and message shape | Retry scheduling or UI |
| `ProtocolEndpoint` | Bounded frame RX/TX, validation, typed event delivery and delivery policy | Request lifecycle transitions |
| `RetryManager` | Bounded transaction retry/ack lifecycle | Claim winner selection |
| `StationApplication` / `CartApplication` | Orchestrate role-specific collaborators in bounded service steps | Direct hardware register access |
| `ButtonEventInterpreter` | Convert debounced physical events into context-valid UI intents | Applying business state transitions |
| UI classes | Navigation state and bounded display view model | Ownership, persistence or protocol decisions |

Do not merge unrelated responsibilities back into a convenience "manager" class. A class may coordinate collaborators, but must not absorb their policies.

## 4. One authoritative assignment writer

There is exactly one assignment-commit authority: `StationClaimCoordinator::commitWinner`.

- `RequestLifecycle` does not expose `commitAssignment`.
- `RequestLedger` stores complete records: full RequestId, destination, creation/expiry basis, lifecycle state, assigned cart, assignment epoch and last accepted journey event sequence.
- The repository exposes only conditional atomic state transitions, including exact-transaction assignment commit and station-time expiry checks.
- The claim coordinator serializes competing claims and invokes this atomic guard. A failed or duplicate claim cannot overwrite the winner.
- The station emits ASSIGN only after the atomic commit succeeds.
- CLAIM, CLAIM_RESULT, transport ACK and retry completion are never proof of ownership.
- Cart activation requires a matching station-originated ASSIGN naming that cart and the exact RequestId/assignment epoch.

The atomic repository guard is mandatory even if the first implementation is single-threaded: retries, duplicate frames, reboot recovery and future concurrency must not create a second winner.

## 5. Claim-disable race policy

Disabling while `CLAIMING` is not an undefined transition.

1. The driver action immediately changes local eligibility to `NOT_ACCEPTING`.
2. The cart clears only its local pending FIFO. It does not broadcast request cancellation.
3. If a claim transaction is unresolved, the cart queues `CLAIM_WITHDRAW` for that transaction and retains the transaction identity until the station resolves it.
4. The station serializes withdrawal against assignment commit:
   - withdrawal committed first: claim is closed without assignment;
   - assignment committed first: the assignment is authoritative and cannot be silently discarded.
5. If ASSIGN already won, the cart processes it, records the assignment and raises a clear driver-facing escalation. It must not pretend the cart was never assigned or automatically re-enable general request acceptance.
6. Any REQUEST received after the local clear may repopulate the FIFO while the cart remains `NOT_ACCEPTING`.

A future protocol implementation must include the withdrawal/result semantics in message validation, retry/idempotency rules and shared golden vectors.

## 6. SOLID design rules

### S — Single Responsibility
Responsibilities and state ownership follow the table in section 3. UI, configuration, request lifecycle, assignment arbitration, protocol and hardware transport remain separate.

### O — Open/Closed
Extend message handling through the message/event catalogue and typed event payloads. New message types must add validation rules, version compatibility behavior, codec vectors and role handlers. Do not add role-specific conditionals to the frame codec. New MCU targets add adapters and composition roots, not forks of domain logic.

### L — Liskov Substitution
Every port and domain abstraction must document preconditions, postconditions, error semantics and bounded execution. An adapter may not weaken guarantees: for example, `send` must not block indefinitely, `pollReceived` must not return malformed unvalidated application events, and persistence must honor atomic assignment semantics. Fakes used in host tests must obey the same contracts. Concrete implementations remain to be verified.

### I — Interface Segregation
Ports are narrow and role dependencies are explicit. Do not make a cart depend on station-only request creation or make a station depend on cart-only driver controls. Split any future interface when a client must implement unused methods.

### D — Dependency Inversion
Applications and use cases receive interfaces/references through composition structs or constructors. Domain code must not instantiate hardware, global singletons, concrete radio libraries or board services. Concrete adapters are wired only at the target composition root.

## 7. Protocol and transport boundary

- `IRadioTransport` sends/receives encoded `FrameView` / `ReceivedFrame` bytes, not domain `ProtocolMessage` objects.
- `FrameCodec` owns the byte-level envelope: protocol version, network ID, message type, source ID, destination/scope, transaction ID, message sequence, payload length, payload and integrity check.
- `MessageValidator` checks decoded envelope compatibility, addressing, sender/message role rules and payload constraints.
- `ProtocolEndpoint` emits a bounded `ProtocolEvent` tagged with a typed payload. The tag determines the valid payload member.
- `DeliveryClass` makes reliability intent explicit at enqueue time.
- ACK means delivery transaction evidence only; it does not mean a person saw a display, a cart physically arrived or a journey completed.
- All queues, frame buffers, event queues and retry slots have explicit configured capacities and overflow errors.

## 8. Scalability and constrained-MCU rules

1. Keep the shared contract layer C++11-compatible for Arduino Uno and the selected ESP32 toolchain.
2. Use fixed-capacity storage in time-critical paths; capacities are validated configuration and bounded by measured RAM.
3. No unbounded heap allocation, recursion, blocking waits or `delay()` in the service path.
4. Each `service()` call performs bounded work and returns; process at most a configured number of RX/TX/events/retries per iteration.
5. Use wrap-safe elapsed-time comparisons for millisecond timers. Station time is authoritative for request expiry.
6. Request identity is the complete tuple `(originStationId, sequence, generation)`; never key by sequence alone.
7. Journey updates carry monotonically increasing event sequence values; stale/duplicate updates are idempotently rejected or acknowledged without state regression.
8. Terminal states cannot be overwritten by delayed REQUEST/CLAIM/JOURNEY_UPDATE messages.
9. Rebooted carts start `NOT_ACCEPTING`, reconcile snapshots and any possible active assignment before claiming.
10. Adding a station/destination is registry/provisioning data, not a new class or branch of code.
11. A new MCU implements the existing ports and passes the same contract/protocol vectors.
12. Memory, stack, airtime, RF capacity and persistence endurance must be measured on each target; no unlimited-scale claim is permitted.

## 9. Build and formatting gates

- Host contract compilation uses CMake and C++11 before hardware libraries are selected.
- Contract tests must cover repository atomic assignment, request expiry/cancel races, claim withdrawal/commit ordering, cart local clear semantics, one-active-assignment invariant, sequence reconciliation, codec bounds and retry exhaustion.
- The shared source is formatted by clang-format using the repository root `.clang-format`.
- Run `sh firmware/campus_assistance/scripts/format.sh` to format and `sh firmware/campus_assistance/scripts/check-format.sh` to verify.
- Enable the tracked local hook with `git config core.hooksPath .githooks`; then make it executable once with `chmod +x .githooks/pre-commit`.
- GitHub Actions independently checks formatting on pull requests.
- A passing host contract compile does not establish Arduino/ESP32 target buildability or hardware correctness.

## 10. Implementation order

1. Implement fixed-capacity value stores and repository semantics; test RequestId and atomic assignment.
2. Implement station lifecycle and claim coordinator; test cancellation/expiry/withdrawal races.
3. Implement cart queue, driver state, active assignment and reconciliation.
4. Implement frame codec/validator with golden vectors shared across targets.
5. Implement protocol endpoint/retry using byte-oriented radio port.
6. Implement role application orchestration and UI intents.
7. Add target-specific adapters and only then select/build exact hardware profiles.

## 11. Review corrections — enforceable distributed invariants

This section supersedes conflicting wording above and is normative.

### 11.1 Repository is a transition boundary, not a mutable record bag

IRequestRepository deliberately has no generic update. Its state-changing methods are conditional atomic operations:

- insertNew
- beginClaimIfEligible
- withdrawClaimIfPending
- commitAssignmentIfClaimPending
- cancelIfUnassigned
- expireIfDue
- applyJourneyUpdateIfCurrent

Each operation must be linearizable for a full RequestId. Assignment commit checks exact cart and claim transaction identity, current claim state PENDING, non-terminal/unassigned request state and station-authoritative expiry in one atomic operation. Cancellation and expiry cannot overwrite an assignment; assignment cannot overwrite cancellation or expiry. The concrete repository is the final invariant guard even when a coordinator performs policy checks first.

### 11.2 Claim withdrawal is a durable transaction

The claim transaction key is (RequestId, cartId, transactionId). Its lifecycle is IDLE -> PENDING -> WITHDRAWN | COMMITTED.

- CartClaimClient exposes request and withdrawal operations and retains the outstanding identity until a final station outcome is known.
- StationClaimCoordinator exposes withdrawal and commit operations and is the only assignment writer.
- withdrawClaimIfPending and commitAssignmentIfClaimPending race on the same persisted record/transaction. Exactly one wins.
- A withdrawal that wins closes that transaction; a later commit from it is rejected.
- A commit that wins is immutable with respect to a delayed withdrawal; the station returns/retransmits the committed ASSIGN outcome.
- Duplicate withdrawal and duplicate commit requests are idempotent and return the existing transaction outcome.
- On reboot, station storage must preserve enough state to reject stale commits and resolve duplicates. If durable recovery cannot prove the transaction state, the station fails closed for that request until reconciliation.

### 11.3 Protocol metadata and trust

Every ProtocolEvent retains the decoded ProtocolHeader and sender-validation level. Applications must not infer sender identity from payload fields when the validated envelope is available. A station-originated ASSIGN is accepted only when envelope source role/ID, network, destination, transaction and message type pass validation and agree with the assignment payload. Network membership validation is not authentication; raw LoRa remains spoofable until cryptographic authentication and key provisioning are implemented.

### 11.4 Cart state ownership

- ActiveAssignment alone owns the cart's active assignment and journey state.
- JourneyProgressController is a stateless policy validator; it receives current and proposed journey status plus the driver action.
- CartClaimClient owns only one outstanding claim transaction and never activates an assignment.
- EventReconciler is stateless orchestration that delegates to ActiveAssignment, DriverAvailabilityController and PendingRequestStore.
- AcceptanceMode and CartServiceState are separate dimensions. Disabling acceptance does not erase a committed assignment and does not stop receiving.

### 11.5 Remaining interface contracts

- RetryManager::pollOutcome reports ACKNOWLEDGED, EXHAUSTED, CANCELLED or STORAGE_FAILURE with transaction ID and attempts used. Retry exhaustion is an explicit event, not an implicit timeout.
- IPersistence::load returns exact/required byte length through an output parameter; BUFFER_FULL never overruns the supplied capacity.
- DestinationConfig and StationRegistry are bounded non-owning views with count <= capacity; caller owns backing storage for the duration of the call.
- ExpiredRequestSet is caller-owned output storage; no component retains its pointer.
- Protocol endpoint composition explicitly injects codec, validator, radio transport and retry manager.
- 16-bit request generations and assignment epochs must not silently wrap/reuse while any record, replay window, retry or recovery snapshot can still refer to the old value. Exhaustion requires persistent namespace rotation or a fail-closed maintenance state.
