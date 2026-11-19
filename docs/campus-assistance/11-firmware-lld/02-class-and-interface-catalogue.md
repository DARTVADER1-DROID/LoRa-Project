# 02 — Class and Interface Catalogue

All declarations are conceptual C++-like contracts. They are not implementation. Use bounded value types and fixed-capacity containers in constrained builds.

## 1. Shared types

    enum class DeviceRole { STATION, CART };
    enum class AcceptanceMode { NOT_ACCEPTING, ACCEPTING };
    enum class CartServiceState { IDLE, CLAIM_PENDING, SERVING, UNCERTAIN };
    enum class RequestState {
      CREATED, PENDING_UNASSIGNED, CLAIM_PENDING, ASSIGNED,
      ON_THE_WAY, ARRIVED_AT_PICKUP, COMPLETED,
      CANCELLED, EXPIRED, COMMUNICATION_UNCERTAIN
    };
    enum class JourneyStatus { RECEIVED, ON_THE_WAY, ARRIVED_AT_PICKUP, COMPLETED };
    enum class ButtonEventType { SHORT_PRESS, LONG_PRESS, RELEASE };
    enum class ErrorCode {
      NONE, INVALID_ARGUMENT, INVALID_STATE, NOT_FOUND, DUPLICATE,
      EXPIRED, CANCELLED, ALREADY_ASSIGNED, NOT_ACCEPTING, BUSY,
      QUEUE_FULL, BUFFER_FULL, TIMEOUT, RADIO_UNAVAILABLE,
      PROTOCOL_MISMATCH, STORAGE_FAILURE, CONFIGURATION_INVALID,
      COMMUNICATION_UNCERTAIN
    };

    struct RequestId {
      DeviceId originStationId;
      RequestSequence sequence;
      RequestGeneration generation;
    };

    struct RequestSummary {
      RequestId id;
      DeviceId destinationId;
      uint32_t localAgeMs;
    };

    struct AssignmentRecord {
      RequestId requestId;
      DeviceId assignedCartId;
      AssignmentEpoch epoch;
      RequestState state;
    };

## 2. Hardware ports

    class IRadioTransport {
    public:
      virtual Status begin(const RadioProfile&) = 0;
      virtual RadioHealth health() const = 0;
      virtual Status send(const FrameView&) = 0;
      virtual bool pollReceived(ReceivedFrame&) = 0;
      virtual void service() = 0;
    };

    class IButtonInput {
    public:
      virtual void begin(const ButtonConfig&) = 0;
      virtual void service(uint32_t nowMs) = 0;
      virtual bool pollEvent(ButtonEvent&) = 0;
    };

    class IDisplay {
    public:
      virtual Status begin() = 0;
      virtual void render(const UiViewModel&) = 0;
    };

    class IBuzzer {
    public:
      virtual Status begin(const BuzzerConfig&) = 0;
      virtual void play(BuzzerCue) = 0;
      virtual void service(uint32_t nowMs) = 0;
    };

    class IClock {
    public:
      virtual uint32_t millisNow() const = 0;
    };

    class IPersistence {
    public:
      virtual Status load(RecordKey, MutableByteSpan) = 0;
      virtual Status storeAtomically(RecordKey, ByteSpan) = 0;
      virtual Status erase(RecordKey) = 0;
    };

    class IDiagnostics {
    public:
      virtual void record(DiagnosticEvent) = 0;
      virtual void increment(CounterId) = 0;
    };

Contract: receive polling and sends are bounded; elapsed-time comparisons are wrap-safe; persistence is wear-aware; UI services never change domain state.

## 3. Identity and bounded queue

    class RequestIdentity {
    public:
      static bool isValid(const RequestId&);
      static bool equals(const RequestId&, const RequestId&);
      static HashValue hash(const RequestId&);
    };

    class PendingRequestStore {
    public:
      Status configure(uint16_t capacity);
      InsertResult insertIfAbsent(const RequestSummary&);
      bool peekOldest(RequestSummary&) const;
      bool remove(const RequestId&);
      uint16_t removeExpired(const ExpiredRequestSet&);
      uint16_t removeAssigned(const RequestId&);
      void clearLocal();
      uint16_t size() const;
      uint16_t capacity() const;
      bool contains(const RequestId&) const;
    };

Invariants: bounded FIFO; full-identity deduplication; duplicate insert does not reorder; overflow never overwrites a live entry; clearLocal has no network side effect.

## 4. Active assignment and driver availability

    class ActiveAssignment {
    public:
      bool isOccupied() const;
      Status activate(const AssignmentRecord& stationConfirmed);
      Status updateJourneyStatus(JourneyStatus, EventSequence);
      Status complete(const DriverAction&);
      Status clearAfterTerminal();
      bool get(AssignmentRecord&) const;
    };

    class DriverAvailabilityController {
    public:
      AcceptanceMode acceptanceMode() const;
      CartServiceState serviceState() const;
      Status enableAcceptance(const DriverAction&);
      Status disableAcceptance(const DriverAction&);
      Status onAssignmentConfirmed(const AssignmentRecord&);
      Status onJourneyCompleted();
      bool mayClaim() const;
    };

Invariants: zero or one active assignment; activation requires matching station confirmation; disable clears the local pending list via application orchestration; completion forces NOT_ACCEPTING; no automatic re-enable.

## 5. Station request authority

    class RequestLifecycle {
    public:
      Status create(const NewRequest&, RequestRecord&);
      Status cancel(const RequestId&, CancellationSource);
      Status expireDue(uint32_t stationNowMs, ExpiredRequestSet&);
      Status applyJourneyUpdate(const JourneyUpdate&);
      bool find(const RequestId&, RequestRecord&) const;
    };

    class IRequestRepository {
    public:
      Status insertNew(const RequestRecord&);
      bool find(const RequestId&, RequestRecord&) const;
      Status beginClaimIfEligible(const RequestId&, DeviceId, TransactionId,
                                  uint32_t stationNowMs, RequestRecord&);
      Status withdrawClaimIfPending(const RequestId&, DeviceId, TransactionId,
                                    RequestRecord&);
      Status commitAssignmentIfClaimPending(const RequestId&, DeviceId,
                                            TransactionId, AssignmentEpoch,
                                            uint32_t stationNowMs, RequestRecord&);
      Status cancelIfUnassigned(const RequestId&, CancellationSource, RequestRecord&);
      Status expireIfDue(const RequestId&, uint32_t stationNowMs, RequestRecord&);
      Status applyJourneyUpdateIfCurrent(const JourneyUpdate&, RequestRecord&);
      Status removeTerminalBefore(uint32_t retentionBoundary);
      uint16_t size() const;
    };

    class DestinationCatalog {
    public:
      Status load(const DestinationConfig&);
      uint16_t count() const;
      bool getByIndex(uint16_t, Destination&) const;
      bool findById(DeviceId, Destination&) const;
      bool isEnabled(DeviceId) const;
    };

There is no generic repository update. Every mutation is a conditional atomic transition against the same authoritative RequestId. RequestLifecycle owns request creation, cancellation, expiry and journey update policy; StationClaimCoordinator alone commits assignment.

## 6. Protocol components

    class FrameCodec {
    public:
      EncodeResult encode(const ProtocolMessage&, MutableByteSpan) const;
      DecodeResult decode(ByteSpan, ProtocolMessage&) const;
    };

    class MessageValidator {
    public:
      ValidationResult validate(const ProtocolMessage&, const DeviceConfig&) const;
    };

    class ProtocolEndpoint {
    public:
      Status begin();
      void service(uint32_t nowMs);
      Status enqueue(const ProtocolMessage&, DeliveryClass);
      bool pollEvent(ProtocolEvent&);
    };

    class RetryManager {
    public:
      Status schedule(const Transmission&, uint32_t nowMs);
      void service(uint32_t nowMs);
      void onAcknowledgement(const Acknowledgement&);
      void cancel(TransactionId);
    };

    class EventReconciler {
    public:
      Status applyAssignmentNotice(const AssignmentNotice&);
      Status applyCancellationNotice(const CancellationNotice&);
      Status applyExpiryNotice(const ExpiryNotice&);
      Status applySnapshot(const RequestSnapshot&);
    };

Protocol handles frames and delivery; domain classes handle business transitions. Retry work is bounded and idempotent.

## 7. Claim coordination

    class CartClaimClient {
    public:
      Status requestClaim(const RequestId&, DeviceId cartId);
      Status withdrawClaim(const ClaimWithdraw&);
      Status onClaimResult(const ClaimResult&);
      bool hasClaimInFlight() const;
      bool getOutstandingClaim(ClaimRequest&) const;
    };

    class StationClaimCoordinator {
    public:
      Status evaluate(const ClaimRequest&, uint32_t nowMs) const;
      Status withdrawClaim(const ClaimWithdraw&);
      Status commitWinner(const ClaimRequest&, AssignmentRecord&);
      bool isAssigned(const RequestId&) const;
    };

The station persists the exact claim transaction identity. Withdrawal and assignment commit serialize against that same record. CartClaimClient never activates an assignment; only EventReconciler may route a validated station ASSIGN to ActiveAssignment.

## 8. Role applications

    class StationApplication {
    public:
      Status begin(const DeviceConfig&);
      void service(uint32_t nowMs);
      void onButtonEvent(const ButtonEvent&);
      void onProtocolEvent(const ProtocolEvent&);
      StationHealth health() const;
    };

    class CartApplication {
    public:
      Status begin(const DeviceConfig&);
      void service(uint32_t nowMs);
      void onButtonEvent(const ButtonEvent&);
      void onProtocolEvent(const ProtocolEvent&);
      CartHealth health() const;
    };

## 9. UI and journey

    class ButtonEventInterpreter {
    public:
      UiIntent interpret(const ButtonEvent&, UiContext) const;
    };

    class StationUserInterface {
    public:
      void onIntent(const UiIntent&);
      UiViewModel view() const;
    };

    class CartUserInterface {
    public:
      void onIntent(const UiIntent&, const CartUiContext&);
      UiViewModel view() const;
    };

    class JourneyProgressController {
    public:
      Status validateTransition(JourneyStatus current, JourneyStatus next,
                                const DriverAction&) const;
    };

ActiveAssignment alone owns the active assignment and current journey state. JourneyProgressController is stateless transition policy. EventReconciler orchestrates authoritative notices and snapshots without maintaining a second copy of assignment/journey state.

## 10. Ownership

One application root owns its collaborators. Request records, queues, frame buffers and retry tables use bounded storage. No decoded-message reference may outlive its receive buffer. Avoid repeated heap allocation.


## 11. Implementation-facing contract corrections

The following C++ headers are authoritative for the current interface scaffold. This section supersedes any older pseudocode above where names or signatures differ.

- `RequestIdentity::hash` returns a 32-bit hash; equality and validity always use the full three-field RequestId.
- `PendingRequestStore::insertIfAbsent` returns `Status`; storage remains bounded and duplicate insertion does not reorder.
- `RequestLifecycle` exposes create, cancel, expiry, sequenced journey update and find. It does **not** commit assignments.
- `IRequestRepository` is the only repository abstraction; concrete storage implementations implement it directly.
- Conditional repository operations serialize claim, withdrawal, assignment, cancellation and expiry; no generic update can bypass them.
- `StationClaimCoordinator::evaluate` returns explicit `Status`; `commitWinner` is the only operation allowed to commit a winner.
- `CartClaimClient::onClaimResult` consumes ClaimResult; `onAssignmentNotice` returns Status and only a matching station assignment can activate the cart.
- `FrameCodec` accepts byte-oriented FrameView/MutableFrameView. `IRadioTransport` never sends domain ProtocolMessage objects.
- `ProtocolEndpoint::enqueue` requires DeliveryClass and emits a tagged ProtocolEvent with a bounded typed payload.
- `RetryManager` schedules a ProtocolMessage with an explicit timestamp; retry capacity and attempt limits are configured and bounded.
- Role applications use StationDependencies/CartDependencies composition structs and explicit collaborator references. Station and cart dependencies are intentionally different.
- `StationRequestController` is separate from ConfigValidator; configuration validation does not submit or cancel requests.
- Driver disable while CLAIMING follows the CLAIM_WITHDRAW race policy in document 06; it is not an unresolved state-machine item.

The scaffold is interface-only. Concrete implementations must satisfy these contracts and the LSP guarantees in document 06.

## 12. Review corrections — authoritative contract

This section supersedes earlier declarations in this catalogue where they conflict.

- IRequestRepository is the repository abstraction. The empty RequestLedger subclass has been removed; a future concrete ledger must implement the repository contract.
- Generic update(RequestRecord) is forbidden. Repository mutations are conditional atomic operations: insertNew, beginClaimIfEligible, withdrawClaimIfPending, commitAssignmentIfClaimPending, cancelIfUnassigned, expireIfDue and applyJourneyUpdateIfCurrent. Each operation must serialize against every competing transition on the same RequestId.
- A claim transaction is identified by the complete tuple (RequestId, cartId, transactionId). A station may commit an assignment only if that exact transaction is still PENDING, the request is still unassigned/non-terminal and the station-authoritative expiry check passes in the same atomic operation.
- StationClaimCoordinator::withdrawClaim and CartClaimClient::withdrawClaim are explicit. Duplicate withdrawal is idempotent; delayed withdrawal cannot undo a committed assignment.
- CartClaimClient owns only the outstanding claim transaction. It does not consume ASSIGN notices or activate assignments.
- ActiveAssignment is the sole cart-side owner of active assignment and journey state. JourneyProgressController::validateTransition(current,next,action) is stateless policy. EventReconciler orchestrates notices into domain owners and stores no duplicate state.
- DriverAvailabilityController exposes AcceptanceMode separately from CartServiceState; acceptance intent is not overloaded with claim/serving/reconciliation state.
- ProtocolEvent retains the full ProtocolHeader and SenderValidation alongside the typed payload. Network membership validation is not cryptographic authentication.
- RetryManager::pollOutcome reports ACKNOWLEDGED, EXHAUSTED, CANCELLED or STORAGE_FAILURE with transaction identity and attempts used.
- IPersistence::load reports the exact or required output length through bytesRead; BUFFER_FULL must not write beyond capacity.
- Destination and station registries are bounded, non-owning configuration views. ExpiredRequestSet is caller-owned storage: items remains valid for the call, capacity is the actual element capacity, and count is output-only.
