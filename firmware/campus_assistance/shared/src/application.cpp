#include "ca/application.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {
namespace {

const uint16_t kDiagnosticInitializationFailure = 1;
const uint16_t kDiagnosticProtocolEventRejected = 2;
const uint16_t kDiagnosticButtonEventRejected = 3;

bool isValidButtonProfile(const ButtonConfig& config) {
  return config.buttonCount > 0 && config.buttonCount <= kMaxPhysicalButtons &&
         config.debounceMs > 0 && config.longPressMs > config.debounceMs;
}

bool isValidBuzzerProfile(const BuzzerConfig& config) {
  return config.defaultToneHz > 0 && config.defaultDurationMs > 0;
}

void put16(uint8_t* output, uint16_t& offset, uint16_t value) {
  output[offset++] = static_cast<uint8_t>(value >> 8);
  output[offset++] = static_cast<uint8_t>(value);
}

void put32(uint8_t* output, uint16_t& offset, uint32_t value) {
  output[offset++] = static_cast<uint8_t>(value >> 24);
  output[offset++] = static_cast<uint8_t>(value >> 16);
  output[offset++] = static_cast<uint8_t>(value >> 8);
  output[offset++] = static_cast<uint8_t>(value);
}

TransactionId transactionFor(const RequestId& id) {
  const TransactionId value = RequestIdentity::hash(id);
  return value == 0 ? 1 : value;
}

TransactionId derivedTransactionFor(const RequestId& id, uint32_t salt) {
  const TransactionId value = RequestIdentity::hash(id) ^ salt;
  return value == 0 ? salt : value;
}

TransactionId assignmentTransactionFor(const RequestId& id) {
  return derivedTransactionFor(id, 0x20000000UL);
}

TransactionId expiryTransactionFor(const RequestId& id) {
  return derivedTransactionFor(id, 0x10000000UL);
}

TransactionId journeyTransactionFor(const RequestId& id, EventSequence sequence) {
  TransactionId value = transactionFor(id) ^ (sequence * 2654435761UL);
  return value == 0 ? 1 : value;
}

TransactionId withdrawalDeliveryTransactionFor(const ClaimRequest& claim) {
  TransactionId value = claim.transactionId ^ 0x80000000UL;
  if (value == 0 || value == claim.transactionId)
    value = claim.transactionId == 0xFFFFFFFFUL ? 1 : claim.transactionId + 1U;
  return value;
}

bool senderIsValidated(SenderValidation validation) {
  return validation == SenderValidation::NETWORK_MEMBER_VALIDATED ||
         validation == SenderValidation::AUTHENTICATED;
}

bool eventBelongsToStation(const ProtocolEvent& event, const RequestId& requestId) {
  return RequestIdentity::isValid(requestId) &&
         event.envelope.sourceDeviceId == requestId.originStationId;
}

Status enqueueClaimResult(ProtocolEndpoint& protocol,
                          const DeviceConfig& config,
                          const ClaimRequest& claim,
                          ErrorCode result,
                          ClaimState finalState) {
  ProtocolMessage message = {};
  message.header.protocolVersion = config.protocolVersion;
  message.header.networkId = config.networkId;
  message.header.messageType = MessageType::CLAIM_RESULT;
  message.header.sourceDeviceId = config.deviceId;
  message.header.destinationDeviceId = claim.cartId;
  message.header.transactionId = claim.transactionId;
  message.header.messageSequence = claim.requestId.sequence;
  uint16_t offset = 0;
  put16(message.payload, offset, claim.requestId.originStationId);
  put32(message.payload, offset, claim.requestId.sequence);
  put16(message.payload, offset, claim.requestId.generation);
  message.payload[offset++] = static_cast<uint8_t>(result);
  message.payload[offset++] = static_cast<uint8_t>(finalState);
  message.payloadLength = offset;
  return protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
}

Status enqueueCartClaim(ProtocolEndpoint& protocol,
                        const DeviceConfig& config,
                        const ClaimRequest& claim) {
  ProtocolMessage message = {};
  message.header.protocolVersion = config.protocolVersion;
  message.header.networkId = config.networkId;
  message.header.messageType = MessageType::CLAIM;
  message.header.sourceDeviceId = config.deviceId;
  message.header.destinationDeviceId = claim.requestId.originStationId;
  message.header.transactionId = claim.transactionId;
  message.header.messageSequence = claim.requestId.sequence;
  uint16_t offset = 0;
  put16(message.payload, offset, claim.requestId.originStationId);
  put32(message.payload, offset, claim.requestId.sequence);
  put16(message.payload, offset, claim.requestId.generation);
  put16(message.payload, offset, claim.cartId);
  message.payloadLength = offset;
  return protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
}

Status enqueueAssignmentNotice(ProtocolEndpoint& protocol,
                               const DeviceConfig& config,
                               const AssignmentRecord& assignment) {
  ProtocolMessage message = {};
  message.header.protocolVersion = config.protocolVersion;
  message.header.networkId = config.networkId;
  message.header.messageType = MessageType::ASSIGN;
  message.header.sourceDeviceId = config.deviceId;
  message.header.destinationDeviceId = 0;
  message.header.transactionId = assignmentTransactionFor(assignment.requestId);
  message.header.messageSequence = assignment.requestId.sequence;
  uint16_t offset = 0;
  put16(message.payload, offset, assignment.requestId.originStationId);
  put32(message.payload, offset, assignment.requestId.sequence);
  put16(message.payload, offset, assignment.requestId.generation);
  put16(message.payload, offset, assignment.assignedCartId);
  put16(message.payload, offset, assignment.epoch);
  message.payload[offset++] = static_cast<uint8_t>(RequestState::ASSIGNED);
  message.payloadLength = offset;
  return protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
}

Status enqueueAuthoritativeSnapshot(ProtocolEndpoint& protocol,
                                    const DeviceConfig& config,
                                    const RequestRecord& record,
                                    EventSequence sequence) {
  if (record.assignedCartId == 0 || record.assignmentEpoch == 0)
    return Status(ErrorCode::INVALID_STATE);

  ProtocolMessage message = {};
  message.header.protocolVersion = config.protocolVersion;
  message.header.networkId = config.networkId;
  message.header.messageType = MessageType::SNAPSHOT;
  message.header.sourceDeviceId = config.deviceId;
  message.header.destinationDeviceId = record.assignedCartId;
  message.header.transactionId = journeyTransactionFor(record.id, sequence);
  message.header.messageSequence = sequence;
  uint16_t offset = 0;
  put16(message.payload, offset, record.id.originStationId);
  put32(message.payload, offset, record.id.sequence);
  put16(message.payload, offset, record.id.generation);
  message.payload[offset++] = static_cast<uint8_t>(record.state);
  put16(message.payload, offset, record.assignedCartId);
  put16(message.payload, offset, record.assignmentEpoch);
  put32(message.payload, offset, record.lastJourneyEventSequence);
  message.payloadLength = offset;
  return protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
}

Status enqueueJourneyUpdate(ProtocolEndpoint& protocol,
                            const DeviceConfig& config,
                            const AssignmentRecord& assignment,
                            JourneyStatus journey,
                            EventSequence sequence) {
  ProtocolMessage message = {};
  message.header.protocolVersion = config.protocolVersion;
  message.header.networkId = config.networkId;
  message.header.messageType = MessageType::JOURNEY_UPDATE;
  message.header.sourceDeviceId = config.deviceId;
  message.header.destinationDeviceId = assignment.requestId.originStationId;
  message.header.transactionId = journeyTransactionFor(assignment.requestId, sequence);
  message.header.messageSequence = sequence;
  uint16_t offset = 0;
  put16(message.payload, offset, assignment.requestId.originStationId);
  put32(message.payload, offset, assignment.requestId.sequence);
  put16(message.payload, offset, assignment.requestId.generation);
  put16(message.payload, offset, assignment.assignedCartId);
  message.payload[offset++] = static_cast<uint8_t>(journey);
  put32(message.payload, offset, sequence);
  message.payloadLength = offset;
  return protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
}

bool currentJourney(const AssignmentRecord& assignment, JourneyStatus& journey) {
  switch (assignment.state) {
  case RequestState::ASSIGNED:
    journey = JourneyStatus::RECEIVED;
    return true;
  case RequestState::ON_THE_WAY:
    journey = JourneyStatus::ON_THE_WAY;
    return true;
  case RequestState::ARRIVED_AT_PICKUP:
    journey = JourneyStatus::ARRIVED_AT_PICKUP;
    return true;
  case RequestState::COMPLETED:
    journey = JourneyStatus::COMPLETED;
    return true;
  default:
    return false;
  }
}

} // namespace

StationApplication::StationApplication(StationDependencies dependencies)
    : dependencies_(dependencies), config_(), expiryOutboxCount_(0),
      expiryOutboxIndex_(0), initialized_(false) {
}

Status StationApplication::begin(const DeviceConfig& config) {
  if (initialized_)
    return Status(ErrorCode::INVALID_STATE);
  if (config.role != DeviceRole::STATION)
    return Status(ErrorCode::CONFIGURATION_INVALID);

  Status status = dependencies_.configValidator.validate(config);
  if (!status.ok())
    return status;
  if (!isValidButtonProfile(dependencies_.buttonConfig) ||
      !isValidBuzzerProfile(dependencies_.buzzerConfig) ||
      dependencies_.expiryBuffer.items == 0 || dependencies_.expiryBuffer.capacity == 0)
    return Status(ErrorCode::CONFIGURATION_INVALID);
  status = dependencies_.requestLifecycle.configure();
  if (!status.ok())
    return status;

  config_ = config;
  status = dependencies_.buttons.begin(dependencies_.buttonConfig);
  if (!status.ok())
    return status;
  status = dependencies_.display.begin();
  if (!status.ok())
    return status;
  status = dependencies_.buzzer.begin(dependencies_.buzzerConfig);
  if (!status.ok())
    return status;
  status = dependencies_.protocol.begin();
  if (!status.ok()) {
    dependencies_.diagnostics.record(kDiagnosticInitializationFailure, status.code);
    return status;
  }

  initialized_ = true;
  return Status();
}

void StationApplication::service() {
  if (!initialized_)
    return;

  const uint32_t nowMs = dependencies_.clock.millisNow();

  if (expiryOutboxIndex_ >= expiryOutboxCount_) {
    dependencies_.expiryBuffer.count = 0;
    const Status expiryStatus =
        dependencies_.requestLifecycle.expireDue(nowMs, dependencies_.expiryBuffer);
    if (!expiryStatus.ok()) {
      dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                       expiryStatus.code);
    } else {
      expiryOutboxCount_ = dependencies_.expiryBuffer.count;
      expiryOutboxIndex_ = 0;
      for (uint16_t i = 0; i < expiryOutboxCount_; ++i)
        dependencies_.requestController.onRequestTerminal(
            dependencies_.expiryBuffer.items[i]);
    }
  }

  while (expiryOutboxIndex_ < expiryOutboxCount_) {
    const RequestId& id = dependencies_.expiryBuffer.items[expiryOutboxIndex_];
    ProtocolMessage message = {};
    message.header.protocolVersion = config_.protocolVersion;
    message.header.networkId = config_.networkId;
    message.header.messageType = MessageType::EXPIRED;
    message.header.sourceDeviceId = config_.deviceId;
    message.header.destinationDeviceId = 0;
    message.header.transactionId = expiryTransactionFor(id);
    message.header.messageSequence = id.sequence;
    uint16_t offset = 0;
    put16(message.payload, offset, id.originStationId);
    put32(message.payload, offset, id.sequence);
    put16(message.payload, offset, id.generation);
    message.payloadLength = offset;
    const Status queued =
        dependencies_.protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
    if (!queued.ok()) {
      dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected, queued.code);
      break;
    }
    ++expiryOutboxIndex_;
  }

  if (expiryOutboxIndex_ >= expiryOutboxCount_) {
    expiryOutboxCount_ = 0;
    expiryOutboxIndex_ = 0;
  }

  const Status cancellationStatus = dependencies_.requestController.service();
  if (!cancellationStatus.ok())
    dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                     cancellationStatus.code);

  dependencies_.buttons.service(nowMs);
  dependencies_.buzzer.service(nowMs);
  dependencies_.protocol.service(nowMs);

  for (uint8_t processed = 0; processed < 4; ++processed) {
    ButtonEvent buttonEvent = {};
    if (!dependencies_.buttons.pollEvent(buttonEvent))
      break;
    onButtonEvent(buttonEvent);
  }

  for (uint8_t processed = 0; processed < 4; ++processed) {
    ProtocolEvent protocolEvent = {};
    if (!dependencies_.protocol.pollEvent(protocolEvent))
      break;
    onProtocolEvent(protocolEvent);
  }

  dependencies_.display.render(dependencies_.ui.view());
}

void StationApplication::onButtonEvent(const ButtonEvent& event) {
  if (!initialized_)
    return;

  UiContext context = {};
  context.screenId = dependencies_.ui.view().screenId;
  context.requestActive = dependencies_.requestController.hasCurrentRequest();
  const UiIntent intent = dependencies_.buttonInterpreter.interpret(event, context);
  if (intent.type != UiIntentType::NONE)
    dependencies_.ui.onIntent(intent);
}

void StationApplication::onProtocolEvent(const ProtocolEvent& event) {
  if (!initialized_)
    return;
  if (!senderIsValidated(event.senderValidation)) {
    dependencies_.diagnostics.increment(kDiagnosticProtocolEventRejected);
    return;
  }

  Status status;
  switch (event.type) {
  case ProtocolEventType::CLAIM_RECEIVED: {
    const ClaimRequest& claim = event.payload.claim;
    if (event.envelope.sourceDeviceId != claim.cartId) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.claims.evaluate(claim, dependencies_.clock.millisNow());
    {
      const bool claimStarted = status.ok();
      bool withdrawalSucceeded = false;
      AssignmentRecord assignment = {};
      if (claimStarted) {
        status = dependencies_.claims.commitWinner(
            claim, dependencies_.clock.millisNow(), assignment);
        if (!status.ok()) {
          ClaimWithdraw withdrawal = {};
          withdrawal.requestId = claim.requestId;
          withdrawal.cartId = claim.cartId;
          withdrawal.transactionId = claim.transactionId;
          withdrawalSucceeded = dependencies_.claims.withdrawClaim(withdrawal).ok();
        }
      }

      const bool assigned =
          dependencies_.claims.getAssignment(claim.requestId, assignment);
      if (assigned) {
        const Status notice =
            enqueueAssignmentNotice(dependencies_.protocol, config_, assignment);
        if (!notice.ok())
          dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                           notice.code);
      }

      const ClaimState finalState =
          assigned ? (assignment.assignedCartId == claim.cartId ? ClaimState::COMMITTED
                                                                : ClaimState::WITHDRAWN)
                   : (claimStarted ? (withdrawalSucceeded ? ClaimState::WITHDRAWN
                                                          : ClaimState::PENDING)
                                   : ClaimState::IDLE);
      const Status response = enqueueClaimResult(
          dependencies_.protocol, config_, claim, status.code, finalState);
      if (!response.ok())
        dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                         response.code);
    }
    break;
  }
  case ProtocolEventType::CLAIM_WITHDRAW_RECEIVED: {
    const ClaimWithdraw& withdrawal = event.payload.claimWithdraw;
    if (event.envelope.sourceDeviceId != withdrawal.cartId) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.claims.withdrawClaim(withdrawal);
    {
      AssignmentRecord assignment = {};
      const bool assigned =
          dependencies_.claims.getAssignment(withdrawal.requestId, assignment);
      if (assigned) {
        const Status notice =
            enqueueAssignmentNotice(dependencies_.protocol, config_, assignment);
        if (!notice.ok())
          dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                           notice.code);
      }
      const ClaimState finalState =
          assigned
              ? (assignment.assignedCartId == withdrawal.cartId ? ClaimState::COMMITTED
                                                                : ClaimState::WITHDRAWN)
              : (status.ok() ? ClaimState::WITHDRAWN : ClaimState::IDLE);
      ClaimRequest claim = {};
      claim.requestId = withdrawal.requestId;
      claim.cartId = withdrawal.cartId;
      claim.transactionId = withdrawal.transactionId;
      const Status response = enqueueClaimResult(
          dependencies_.protocol, config_, claim, status.code, finalState);
      if (!response.ok())
        dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                         response.code);
    }
    break;
  }
  case ProtocolEventType::JOURNEY_UPDATE_RECEIVED: {
    const JourneyUpdate& update = event.payload.journeyUpdate;
    if (event.envelope.sourceDeviceId != update.assignedCartId ||
        !RequestIdentity::isValid(update.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.requestLifecycle.applyJourneyUpdate(update);
    if (status.ok() || status.code == ErrorCode::DUPLICATE) {
      RequestRecord record = {};
      if (!dependencies_.requestLifecycle.find(update.requestId, record)) {
        status = Status(ErrorCode::NOT_FOUND);
        break;
      }
      const Status snapshot = enqueueAuthoritativeSnapshot(
          dependencies_.protocol, config_, record, update.eventSequence);
      if (!snapshot.ok()) {
        status = snapshot;
        break;
      }
      if (record.state == RequestState::COMPLETED)
        dependencies_.requestController.onRequestTerminal(update.requestId);
    }
    break;
  }
  case ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED:
    status = Status();
    break;
  default:
    status = Status(ErrorCode::PROTOCOL_MISMATCH);
    break;
  }

  if (!status.ok()) {
    dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected, status.code);
  }
}

bool StationApplication::isHealthy() const {
  return initialized_;
}

CartApplication::CartApplication(CartDependencies dependencies)
    : dependencies_(dependencies), config_(), initialized_(false) {
}

Status CartApplication::begin(const DeviceConfig& config) {
  if (initialized_)
    return Status(ErrorCode::INVALID_STATE);
  if (config.role != DeviceRole::CART)
    return Status(ErrorCode::CONFIGURATION_INVALID);

  Status status = dependencies_.configValidator.validate(config);
  if (!status.ok())
    return status;
  if (!isValidButtonProfile(dependencies_.buttonConfig) ||
      !isValidBuzzerProfile(dependencies_.buzzerConfig))
    return Status(ErrorCode::CONFIGURATION_INVALID);

  status = dependencies_.pendingRequests.configure(config.pendingCapacity);
  if (!status.ok())
    return status;

  config_ = config;
  status = dependencies_.buttons.begin(dependencies_.buttonConfig);
  if (!status.ok())
    return status;
  status = dependencies_.display.begin();
  if (!status.ok())
    return status;
  status = dependencies_.buzzer.begin(dependencies_.buzzerConfig);
  if (!status.ok())
    return status;
  status = dependencies_.protocol.begin();
  if (!status.ok()) {
    dependencies_.diagnostics.record(kDiagnosticInitializationFailure, status.code);
    return status;
  }

  initialized_ = true;
  return Status();
}

void CartApplication::service() {
  if (!initialized_)
    return;

  const uint32_t nowMs = dependencies_.clock.millisNow();
  dependencies_.buttons.service(nowMs);
  dependencies_.buzzer.service(nowMs);
  dependencies_.protocol.service(nowMs);

  for (uint8_t processed = 0; processed < 4; ++processed) {
    ButtonEvent buttonEvent = {};
    if (!dependencies_.buttons.pollEvent(buttonEvent))
      break;
    onButtonEvent(buttonEvent);
  }

  for (uint8_t processed = 0; processed < 4; ++processed) {
    ProtocolEvent protocolEvent = {};
    if (!dependencies_.protocol.pollEvent(protocolEvent))
      break;
    onProtocolEvent(protocolEvent);
  }

  dependencies_.display.render(dependencies_.ui.view());
}

void CartApplication::onButtonEvent(const ButtonEvent& event) {
  if (!initialized_)
    return;

  CartUiContext context = {};
  context.screenId = dependencies_.ui.view().screenId;
  context.acceptanceMode = dependencies_.driverAvailability.acceptanceMode();
  context.serviceState = dependencies_.driverAvailability.serviceState();
  context.activeAssignment = dependencies_.activeAssignment.isOccupied();

  const UiIntent intent = dependencies_.buttonInterpreter.interpret(
      event, UiContext{context.screenId, context.activeAssignment});
  if (intent.type == UiIntentType::ADVANCE_JOURNEY ||
      intent.type == UiIntentType::CONFIRM_COMPLETION) {
    AssignmentRecord assignment = {};
    JourneyStatus current = JourneyStatus::RECEIVED;
    if (!dependencies_.activeAssignment.get(assignment) ||
        !currentJourney(assignment, current)) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       ErrorCode::INVALID_STATE);
      return;
    }

    const bool completing = intent.type == UiIntentType::CONFIRM_COMPLETION;
    const JourneyStatus next = completing ? JourneyStatus::COMPLETED
                                          : (current == JourneyStatus::RECEIVED
                                                 ? JourneyStatus::ON_THE_WAY
                                                 : JourneyStatus::ARRIVED_AT_PICKUP);
    const DriverAction action = {completing ? DriverActionType::CONFIRM_COMPLETION
                                            : DriverActionType::ADVANCE_JOURNEY,
                                 event.timestampMs};
    Status status =
        dependencies_.journeyProgress.validateTransition(current, next, action);
    if (!status.ok()) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
      return;
    }

    const EventSequence nextSequence = assignment.lastEventSequence + 1U;
    status = enqueueJourneyUpdate(
        dependencies_.protocol, config_, assignment, next, nextSequence);
    if (!status.ok()) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
      return;
    }

    if (completing) {
      status = dependencies_.activeAssignment.complete(action);
      if (status.ok())
        status = dependencies_.activeAssignment.updateJourneyStatus(next, nextSequence);
      if (status.ok())
        status = dependencies_.driverAvailability.onJourneyCompleted();
    } else {
      status = dependencies_.activeAssignment.updateJourneyStatus(next, nextSequence);
    }
    if (!status.ok())
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
    return;
  }

  if (intent.type == UiIntentType::CONFIRM) {
    if (!dependencies_.driverAvailability.mayClaim()) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       ErrorCode::NOT_ACCEPTING);
      return;
    }

    RequestSummary pending = {};
    if (!dependencies_.pendingRequests.peekOldest(pending)) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       ErrorCode::NOT_FOUND);
      return;
    }

    Status status =
        dependencies_.claimClient.requestClaim(pending.id, config_.deviceId);
    if (!status.ok()) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
      return;
    }

    ClaimRequest claim = {};
    if (!dependencies_.claimClient.getOutstandingClaim(claim)) {
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       ErrorCode::INVALID_STATE);
      return;
    }

    status = dependencies_.driverAvailability.onClaimStarted();
    if (!status.ok()) {
      ClaimResult rejected = {};
      rejected.requestId = claim.requestId;
      rejected.transactionId = claim.transactionId;
      rejected.result = status.code;
      rejected.finalState = ClaimState::IDLE;
      dependencies_.claimClient.onClaimResult(rejected);
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
      return;
    }

    status = enqueueCartClaim(dependencies_.protocol, config_, claim);
    if (!status.ok()) {
      ClaimResult rejected = {};
      rejected.requestId = claim.requestId;
      rejected.transactionId = claim.transactionId;
      rejected.result = status.code;
      rejected.finalState = ClaimState::IDLE;
      dependencies_.claimClient.onClaimResult(rejected);
      dependencies_.driverAvailability.onClaimWithdrawnByStation();
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
    }
    return;
  }

  if (intent.type == UiIntentType::ENABLE_ACCEPTANCE) {
    DriverAction action = {DriverActionType::ENABLE_ACCEPTANCE, event.timestampMs};
    const Status status = dependencies_.driverAvailability.enableAcceptance(action);
    if (!status.ok())
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);
    return;
  }
  if (intent.type == UiIntentType::DISABLE_ACCEPTANCE) {
    DriverAction action = {DriverActionType::DISABLE_ACCEPTANCE, event.timestampMs};
    const Status status = dependencies_.driverAvailability.disableAcceptance(action);
    dependencies_.pendingRequests.clearLocal();
    if (!status.ok() && status.code != ErrorCode::COMMUNICATION_UNCERTAIN)
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected, status.code);

    ClaimRequest claim = {};
    if (dependencies_.claimClient.getOutstandingClaim(claim)) {
      ClaimWithdraw withdrawal = {};
      withdrawal.requestId = claim.requestId;
      withdrawal.cartId = claim.cartId;
      withdrawal.transactionId = claim.transactionId;
      const Status withdrawalStarted =
          dependencies_.claimClient.withdrawClaim(withdrawal);
      if (!withdrawalStarted.ok()) {
        dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                         withdrawalStarted.code);
        return;
      }
      ProtocolMessage message = {};
      message.header.protocolVersion = config_.protocolVersion;
      message.header.networkId = config_.networkId;
      message.header.messageType = MessageType::CLAIM_WITHDRAW;
      message.header.sourceDeviceId = config_.deviceId;
      message.header.destinationDeviceId = claim.requestId.originStationId;
      message.header.transactionId =
          withdrawalDeliveryTransactionFor(claim);
      message.header.messageSequence = claim.requestId.sequence;
      message.payloadLength = 14;
      uint16_t offset = 0;
      put16(message.payload, offset, claim.requestId.originStationId);
      put32(message.payload, offset, claim.requestId.sequence);
      put16(message.payload, offset, claim.requestId.generation);
      put16(message.payload, offset, claim.cartId);
      put32(message.payload, offset, claim.transactionId);
      const Status withdrawalQueued =
          dependencies_.protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
      if (!withdrawalQueued.ok())
        dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                         withdrawalQueued.code);
    }
    return;
  }

  dependencies_.ui.onIntent(intent, context);
}

void CartApplication::onProtocolEvent(const ProtocolEvent& event) {
  if (!initialized_)
    return;
  if (!senderIsValidated(event.senderValidation)) {
    dependencies_.diagnostics.increment(kDiagnosticProtocolEventRejected);
    return;
  }

  Status status;
  switch (event.type) {
  case ProtocolEventType::REQUEST_RECEIVED: {
    const RequestSummary& request = event.payload.request;
    if (!RequestIdentity::isValid(request.id) || request.destinationId == 0 ||
        event.envelope.sourceDeviceId != request.id.originStationId) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    if (request.localAgeMs >= config_.requestLifetimeMs) {
      status = Status(ErrorCode::EXPIRED);
      break;
    }
    status = dependencies_.pendingRequests.insertIfAbsent(request);
    break;
  }
  case ProtocolEventType::ASSIGNMENT_RECEIVED: {
    const AssignmentNotice& notice = event.payload.assignment;
    if (!eventBelongsToStation(event, notice.assignment.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.reconciler.applyAssignmentNotice(notice);
    break;
  }
  case ProtocolEventType::CANCELLATION_RECEIVED: {
    const CancellationNotice& notice = event.payload.cancellation;
    if (!eventBelongsToStation(event, notice.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.reconciler.applyCancellationNotice(notice);
    break;
  }
  case ProtocolEventType::EXPIRY_RECEIVED: {
    const ExpiryNotice& notice = event.payload.expiry;
    if (!eventBelongsToStation(event, notice.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.reconciler.applyExpiryNotice(notice);
    break;
  }
  case ProtocolEventType::SNAPSHOT_RECEIVED: {
    const RequestSnapshot& snapshot = event.payload.snapshot;
    if (!eventBelongsToStation(event, snapshot.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.reconciler.applySnapshot(snapshot);
    break;
  }
  case ProtocolEventType::CLAIM_RESULT_RECEIVED:
    if (!eventBelongsToStation(event, event.payload.claimResult.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.claimClient.onClaimResult(event.payload.claimResult);
    if (status.code == ErrorCode::STALE_EVENT &&
        event.payload.claimResult.finalState == ClaimState::COMMITTED) {
      AssignmentRecord active = {};
      if (dependencies_.activeAssignment.get(active) &&
          RequestIdentity::equals(active.requestId,
                                  event.payload.claimResult.requestId))
        status = Status();
    }
    if (status.ok() &&
        (event.payload.claimResult.finalState == ClaimState::WITHDRAWN ||
         (event.payload.claimResult.result != ErrorCode::NONE &&
          (event.payload.claimResult.finalState == ClaimState::IDLE ||
           event.payload.claimResult.finalState == ClaimState::WITHDRAWN)))) {
      const Status resolved =
          dependencies_.driverAvailability.onClaimWithdrawnByStation();
      if (!resolved.ok() && resolved.code != ErrorCode::INVALID_STATE)
        dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                         resolved.code);
    }
    break;
  case ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED:
    status = Status();
    break;
  default:
    status = Status(ErrorCode::PROTOCOL_MISMATCH);
    break;
  }

  if (!status.ok() && status.code != ErrorCode::DUPLICATE &&
      status.code != ErrorCode::EXPIRED) {
    dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected, status.code);
  }
}

AcceptanceMode CartApplication::acceptanceMode() const {
  return dependencies_.driverAvailability.acceptanceMode();
}

CartServiceState CartApplication::serviceState() const {
  return dependencies_.driverAvailability.serviceState();
}

bool CartApplication::hasActiveAssignment() const {
  return dependencies_.activeAssignment.isOccupied();
}

} // namespace ca
