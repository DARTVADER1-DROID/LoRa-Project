#include "CampusAssistance.h"

#include <assert.h>
#include <string.h>

using namespace ca;

class FakeClock : public IClock {
public:
  FakeClock() : now_(100) {}
  uint32_t millisNow() const override { return now_; }
  void set(uint32_t value) { now_ = value; }
private:
  uint32_t now_;
};

class FakeButtons : public IButtonInput {
public:
  FakeButtons() : begun_(false), services_(0) {}
  Status begin(const ButtonConfig&) override { begun_ = true; return Status(); }
  void service(uint32_t) override { ++services_; }
  bool pollEvent(ButtonEvent&) override { return false; }
  bool begun_;
  uint16_t services_;
};

class FakeDisplay : public IDisplay {
public:
  FakeDisplay() : begun_(false), renders_(0) { memset(&last_, 0, sizeof(last_)); }
  Status begin() override { begun_ = true; return Status(); }
  void render(const UiViewModel& view) override { last_ = view; ++renders_; }
  bool begun_;
  uint16_t renders_;
  UiViewModel last_;
};

class FakeBuzzer : public IBuzzer {
public:
  FakeBuzzer() : begun_(false), cues_(0), services_(0) {}
  Status begin(const BuzzerConfig&) override { begun_ = true; return Status(); }
  void playCue(uint8_t) override { ++cues_; }
  void service(uint32_t) override { ++services_; }
  bool begun_;
  uint16_t cues_;
  uint16_t services_;
};

class FakeDiagnostics : public IDiagnostics {
public:
  FakeDiagnostics() : records_(0), increments_(0), lastError_(ErrorCode::NONE) {}
  void record(uint16_t, ErrorCode error) override { ++records_; lastError_ = error; }
  void increment(uint16_t) override { ++increments_; }
  uint16_t records_;
  uint16_t increments_;
  ErrorCode lastError_;
};

class FakeProtocol : public ProtocolEndpoint {
public:
  FakeProtocol() : begun_(false), count_(0) {}
  Status begin() override { begun_ = true; return Status(); }
  void service(uint32_t) override {}
  Status enqueue(const ProtocolMessage& message, DeliveryClass) override {
    if (count_ >= 16)
      return Status(ErrorCode::QUEUE_FULL);
    messages_[count_++] = message;
    return Status();
  }
  bool pollEvent(ProtocolEvent&) override { return false; }
  bool pollRetryOutcome(RetryOutcome&) override { return false; }
  const ProtocolMessage& last() const { assert(count_ > 0); return messages_[count_ - 1]; }
  uint8_t count() const { return count_; }
  bool begun_;
private:
  ProtocolMessage messages_[16];
  uint8_t count_;
};

static DeviceConfig makeConfig(DeviceRole role, DeviceId id) {
  DeviceConfig config = {};
  config.role = role;
  config.deviceId = id;
  config.networkId = 7;
  config.protocolVersion = 1;
  config.radioProfileId = 1;
  config.boardProfileId = 1;
  config.pendingCapacity = 4;
  config.requestLifetimeMs = 600000UL;
  return config;
}

static ProtocolEvent validatedEvent(ProtocolEventType type, DeviceId sender) {
  ProtocolEvent event = {};
  event.type = type;
  event.envelope.sourceDeviceId = sender;
  event.senderValidation = SenderValidation::NETWORK_MEMBER_VALIDATED;
  return event;
}

int main() {
  const ButtonConfig buttonConfig = {2, 20, 700};
  const BuzzerConfig buzzerConfig = {2200, 100};
  FakeClock clock;
  DefaultConfigValidator stationValidator;
  DefaultConfigValidator cartValidator;

  // Station composition: repository -> lifecycle -> request controller ->
  // station UI -> application, plus the authoritative claim coordinator.
  Destination destinationsData[1] = {};
  destinationsData[0].id = 3;
  destinationsData[0].enabled = true;
  strcpy(destinationsData[0].label, "Library");
  FixedDestinationCatalog<2> destinations;
  assert(destinations.load({1, destinationsData, 1}).ok());

  FixedRequestRepository<8> repository;
  assert(repository.configure().ok());
  StationRequestLifecycle lifecycle(repository, destinations, clock, 1, 600000UL);
  FakeProtocol stationProtocol;
  StationRequestController requestController(
      destinations, lifecycle, stationProtocol, makeConfig(DeviceRole::STATION, 1));
  RepositoryStationClaimCoordinator claims(repository);
  BasicStationUserInterface stationUi(destinations, requestController);
  TwoButtonEventInterpreter stationInterpreter(DeviceRole::STATION);
  FakeButtons stationButtons;
  FakeDisplay stationDisplay;
  FakeBuzzer stationBuzzer;
  FakeDiagnostics stationDiagnostics;
  RequestId expiryItems[8] = {};
  ExpiredRequestSet expiryBuffer = {0, expiryItems, 8};

  StationDependencies stationDependencies = {
      buttonConfig, buzzerConfig, clock, stationButtons, stationDisplay,
      stationBuzzer, stationDiagnostics, stationValidator, stationInterpreter,
      stationProtocol, lifecycle, expiryBuffer, requestController, claims, stationUi};
  StationApplication station(stationDependencies);
  const DeviceConfig stationConfig = makeConfig(DeviceRole::STATION, 1);
  assert(station.begin(stationConfig).ok());
  assert(station.isHealthy());
  assert(stationButtons.begun_ && stationDisplay.begun_ &&
         stationBuzzer.begun_ && stationProtocol.begun_);

  // Driver/station button event creates one station-owned request.
  ButtonEvent confirmDestination = {1, ButtonEventType::SHORT_PRESS, 110};
  station.onButtonEvent(confirmDestination);
  assert(requestController.hasCurrentRequest());
  RequestRecord created = {};
  assert(repository.recordAt(0, created));
  assert(created.destinationId == 3);
  assert(stationProtocol.last().header.messageType == MessageType::REQUEST);

  // Cart composition: queue + claim + assignment + journey + UI + application.
  FakeProtocol cartProtocol;
  FakeButtons cartButtons;
  FakeDisplay cartDisplay;
  FakeBuzzer cartBuzzer;
  FakeDiagnostics cartDiagnostics;
  TwoButtonEventInterpreter cartInterpreter(DeviceRole::CART);
  FixedPendingRequestStore<4> pending;
  FixedActiveAssignment active;
  BasicDriverAvailabilityController availability;
  FixedCartClaimClient claimClient(7, 100);
  DefaultJourneyProgressController journeyProgress;
  CartEventReconciler reconciler(7, pending, active, availability, claimClient);
  BasicCartUserInterface cartUi;

  CartDependencies cartDependencies = {
      buttonConfig, buzzerConfig, clock, cartButtons, cartDisplay, cartBuzzer,
      cartDiagnostics, cartValidator, cartInterpreter, cartProtocol, pending,
      active, availability, journeyProgress, claimClient, reconciler, cartUi};
  CartApplication cart(cartDependencies);
  const DeviceConfig cartConfig = makeConfig(DeviceRole::CART, 7);
  assert(cart.begin(cartConfig).ok());
  assert(cartButtons.begun_ && cartDisplay.begun_ &&
         cartBuzzer.begun_ && cartProtocol.begun_);

  ProtocolEvent request = validatedEvent(ProtocolEventType::REQUEST_RECEIVED, 1);
  request.payload.request.id = created.id;
  request.payload.request.destinationId = created.destinationId;
  request.payload.request.localAgeMs = 0;

  // Reject unvalidated, misattributed and expired requests before queueing.
  ProtocolEvent untrustedRequest = request;
  untrustedRequest.senderValidation = SenderValidation::UNVALIDATED;
  cart.onProtocolEvent(untrustedRequest);
  assert(pending.size() == 0);
  ProtocolEvent spoofedRequest = request;
  spoofedRequest.envelope.sourceDeviceId = 9;
  cart.onProtocolEvent(spoofedRequest);
  assert(pending.size() == 0);
  ProtocolEvent expiredRequest = request;
  expiredRequest.payload.request.localAgeMs = cartConfig.requestLifetimeMs;
  cart.onProtocolEvent(expiredRequest);
  assert(pending.size() == 0);

  cart.onProtocolEvent(request);
  assert(pending.size() == 1);
  cart.onProtocolEvent(request); // retransmission is idempotent at the queue boundary
  assert(pending.size() == 1);
  assert(pending.contains(created.id));

  // Driver explicitly enables acceptance and requests the oldest job.
  ButtonEvent enable = {0, ButtonEventType::LONG_PRESS, 120};
  cart.onButtonEvent(enable);
  assert(cart.acceptanceMode() == AcceptanceMode::ACCEPTING);
  ButtonEvent claim = {1, ButtonEventType::SHORT_PRESS, 130};
  cart.onButtonEvent(claim);
  assert(cart.serviceState() == CartServiceState::CLAIM_PENDING);
  assert(claimClient.hasClaimInFlight());
  assert(cartProtocol.last().header.messageType == MessageType::CLAIM);

  ClaimRequest claimRequest = {};
  assert(claimClient.getOutstandingClaim(claimRequest));
  ProtocolEvent claimReceived = validatedEvent(ProtocolEventType::CLAIM_RECEIVED, 7);
  claimReceived.payload.claim = claimRequest;
  ProtocolEvent spoofedClaim = claimReceived;
  spoofedClaim.envelope.sourceDeviceId = 8;
  station.onProtocolEvent(spoofedClaim);
  AssignmentRecord committed = {};
  assert(!claims.getAssignment(created.id, committed));
  station.onProtocolEvent(claimReceived);

  // The station commits one winner and broadcasts an authoritative assignment.
  assert(claims.getAssignment(created.id, committed));
  assert(committed.assignedCartId == 7);
  assert(committed.epoch == 1);
  assert(stationProtocol.last().header.messageType == MessageType::CLAIM_RESULT ||
         stationProtocol.last().header.messageType == MessageType::ASSIGN);

  ProtocolEvent assignment = validatedEvent(ProtocolEventType::ASSIGNMENT_RECEIVED, 1);
  assignment.payload.assignment.assignment = committed;
  ProtocolEvent wrongStationAssignment = assignment;
  wrongStationAssignment.envelope.sourceDeviceId = 2;
  cart.onProtocolEvent(wrongStationAssignment);
  assert(!active.isOccupied());
  cart.onProtocolEvent(assignment);
  assert(active.isOccupied());
  assert(cart.hasActiveAssignment());
  assert(cart.serviceState() == CartServiceState::SERVING);
  assert(cart.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);
  assert(!pending.contains(created.id));

  // Journey advances only through the allowed ordered transitions.
  ButtonEvent advance = {1, ButtonEventType::LONG_PRESS, 200};
  cart.onButtonEvent(advance);
  AssignmentRecord cartAssignment = {};
  assert(active.get(cartAssignment));
  assert(cartAssignment.state == RequestState::ON_THE_WAY);
  assert(cartProtocol.last().header.messageType == MessageType::JOURNEY_UPDATE);

  JourneyUpdate update = {};
  update.requestId = created.id;
  update.assignedCartId = 7;
  update.status = JourneyStatus::ON_THE_WAY;
  update.eventSequence = 1;
  ProtocolEvent journeyEvent = validatedEvent(ProtocolEventType::JOURNEY_UPDATE_RECEIVED, 7);
  journeyEvent.payload.journeyUpdate = update;
  station.onProtocolEvent(journeyEvent);
  RequestRecord stationRecord = {};
  assert(lifecycle.find(created.id, stationRecord));
  assert(stationRecord.state == RequestState::ON_THE_WAY);

  advance.timestampMs = 300;
  cart.onButtonEvent(advance);
  assert(active.get(cartAssignment));
  assert(cartAssignment.state == RequestState::ARRIVED_AT_PICKUP);
  update.status = JourneyStatus::ARRIVED_AT_PICKUP;
  update.eventSequence = 2;
  journeyEvent.payload.journeyUpdate = update;
  station.onProtocolEvent(journeyEvent);
  assert(lifecycle.find(created.id, stationRecord));
  assert(stationRecord.state == RequestState::ARRIVED_AT_PICKUP);

  advance.timestampMs = 400;
  cart.onButtonEvent(advance);
  assert(active.get(cartAssignment));
  assert(cartAssignment.state == RequestState::COMPLETED);
  assert(cart.serviceState() == CartServiceState::IDLE);
  assert(cart.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);
  update.status = JourneyStatus::COMPLETED;
  update.eventSequence = 3;
  journeyEvent.payload.journeyUpdate = update;
  station.onProtocolEvent(journeyEvent);
  assert(lifecycle.find(created.id, stationRecord));
  assert(stationRecord.state == RequestState::COMPLETED);
  assert(!requestController.hasCurrentRequest());

  // A station-authoritative completed snapshot clears the cart's terminal
  // local assignment only after reconciliation.
  RequestSnapshot snapshot = {};
  snapshot.requestId = created.id;
  snapshot.state = RequestState::COMPLETED;
  snapshot.assignedCartId = 7;
  snapshot.assignmentEpoch = 1;
  snapshot.lastEventSequence = 3;
  snapshot.claimState = ClaimState::COMMITTED;
  snapshot.claimCartId = 7;
  snapshot.claimTransactionId = claimRequest.transactionId;
  ProtocolEvent snapshotEvent = validatedEvent(ProtocolEventType::SNAPSHOT_RECEIVED, 1);
  snapshotEvent.payload.snapshot = snapshot;
  cart.onProtocolEvent(snapshotEvent);
  assert(!cart.hasActiveAssignment());
  assert(cart.serviceState() == CartServiceState::IDLE);
  assert(cart.acceptanceMode() == AcceptanceMode::NOT_ACCEPTING);

  // Both event loops continue servicing their injected dependencies.
  station.service();
  cart.service();
  assert(stationButtons.services_ == 1);
  assert(cartButtons.services_ == 1);
  assert(stationDisplay.renders_ == 1);
  assert(cartDisplay.renders_ == 1);
  return 0;
}
