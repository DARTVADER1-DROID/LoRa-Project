#include "ca/application.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {
namespace {

const uint16_t kDiagnosticInitializationFailure = 1;
const uint16_t kDiagnosticProtocolEventRejected = 2;
const uint16_t kDiagnosticButtonEventRejected = 3;

bool senderIsValidated(SenderValidation validation) {
  return validation == SenderValidation::NETWORK_MEMBER_VALIDATED ||
         validation == SenderValidation::AUTHENTICATED;
}

bool eventBelongsToStation(const ProtocolEvent& event,
                           const RequestId& requestId) {
  return RequestIdentity::isValid(requestId) &&
         event.envelope.sourceDeviceId == requestId.originStationId;
}

} // namespace

StationApplication::StationApplication(StationDependencies dependencies)
    : dependencies_(dependencies), config_(), initialized_(false) {}

Status StationApplication::begin(const DeviceConfig& config) {
  if (initialized_)
    return Status(ErrorCode::INVALID_STATE);
  if (config.role != DeviceRole::STATION)
    return Status(ErrorCode::CONFIGURATION_INVALID);

  Status status = dependencies_.configValidator.validate(config);
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
    dependencies_.diagnostics.record(kDiagnosticInitializationFailure,
                                     status.code);
    return status;
  }

  initialized_ = true;
  return Status();
}

void StationApplication::service() {
  if (!initialized_)
    return;

  const uint32_t nowMs = dependencies_.clock.millisNow();
  dependencies_.buttons.service(nowMs);
  dependencies_.buzzer.service(nowMs);
  dependencies_.protocol.service(nowMs);

  ButtonEvent buttonEvent = {};
  if (dependencies_.buttons.pollEvent(buttonEvent))
    onButtonEvent(buttonEvent);

  ProtocolEvent protocolEvent = {};
  if (dependencies_.protocol.pollEvent(protocolEvent))
    onProtocolEvent(protocolEvent);

  dependencies_.display.render(dependencies_.ui.view());
}

void StationApplication::onButtonEvent(const ButtonEvent& event) {
  if (!initialized_)
    return;

  UiContext context = {};
  context.screenId = dependencies_.ui.view().screenId;
  context.requestActive = false;
  const UiIntent intent =
      dependencies_.buttonInterpreter.interpret(event, context);
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
    status = dependencies_.claims.evaluate(
        claim, dependencies_.clock.millisNow());
    break;
  }
  case ProtocolEventType::CLAIM_WITHDRAW_RECEIVED: {
    const ClaimWithdraw& withdrawal = event.payload.claimWithdraw;
    if (event.envelope.sourceDeviceId != withdrawal.cartId) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.claims.withdrawClaim(withdrawal);
    break;
  }
  case ProtocolEventType::JOURNEY_UPDATE_RECEIVED: {
    const JourneyUpdate& update = event.payload.journeyUpdate;
    if (event.envelope.sourceDeviceId != update.assignedCartId ||
        !eventBelongsToStation(event, update.requestId)) {
      status = Status(ErrorCode::INVALID_ARGUMENT);
      break;
    }
    status = dependencies_.requestController.cancelCurrentRequest().code ==
                     ErrorCode::NOT_IMPLEMENTED
                 ? Status(ErrorCode::INVALID_STATE)
                 : Status(ErrorCode::NOT_IMPLEMENTED);
    // Journey updates are applied by the station lifecycle owner. The current
    // application dependency exposes the request controller, not that owner.
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
    dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                     status.code);
  }
}

bool StationApplication::isHealthy() const { return initialized_; }

CartApplication::CartApplication(CartDependencies dependencies)
    : dependencies_(dependencies), config_(), initialized_(false) {}

Status CartApplication::begin(const DeviceConfig& config) {
  if (initialized_)
    return Status(ErrorCode::INVALID_STATE);
  if (config.role != DeviceRole::CART)
    return Status(ErrorCode::CONFIGURATION_INVALID);

  Status status = dependencies_.configValidator.validate(config);
  if (!status.ok())
    return status;

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
    dependencies_.diagnostics.record(kDiagnosticInitializationFailure,
                                     status.code);
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

  ButtonEvent buttonEvent = {};
  if (dependencies_.buttons.pollEvent(buttonEvent))
    onButtonEvent(buttonEvent);

  ProtocolEvent protocolEvent = {};
  if (dependencies_.protocol.pollEvent(protocolEvent))
    onProtocolEvent(protocolEvent);

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

  const UiIntent intent =
      dependencies_.buttonInterpreter.interpret(
          event, UiContext{context.screenId, context.activeAssignment});
  if (intent.type == UiIntentType::ENABLE_ACCEPTANCE) {
    DriverAction action = {DriverActionType::ENABLE_ACCEPTANCE,
                           event.timestampMs};
    const Status status = dependencies_.driverAvailability.enableAcceptance(action);
    if (!status.ok())
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       status.code);
    return;
  }
  if (intent.type == UiIntentType::DISABLE_ACCEPTANCE) {
    DriverAction action = {DriverActionType::DISABLE_ACCEPTANCE,
                           event.timestampMs};
    const Status status = dependencies_.driverAvailability.disableAcceptance(action);
    dependencies_.pendingRequests.clearLocal();
    if (!status.ok() && status.code != ErrorCode::COMMUNICATION_UNCERTAIN)
      dependencies_.diagnostics.record(kDiagnosticButtonEventRejected,
                                       status.code);

    ClaimRequest claim = {};
    if (dependencies_.claimClient.getOutstandingClaim(claim)) {
      ClaimWithdraw withdrawal = {};
      withdrawal.requestId = claim.requestId;
      withdrawal.cartId = claim.cartId;
      withdrawal.transactionId = claim.transactionId;
      dependencies_.claimClient.withdrawClaim(withdrawal);
      ProtocolMessage message = {};
      message.header.protocolVersion = config_.protocolVersion;
      message.header.networkId = config_.networkId;
      message.header.messageType = MessageType::CLAIM_WITHDRAW;
      message.header.sourceDeviceId = config_.deviceId;
      message.header.destinationDeviceId = claim.requestId.originStationId;
      message.header.transactionId = claim.transactionId;
      message.header.messageSequence = claim.requestId.sequence;
      message.payloadLength = 12;
      uint16_t offset = 0;
      message.payload[offset++] = static_cast<uint8_t>(claim.requestId.originStationId >> 8);
      message.payload[offset++] = static_cast<uint8_t>(claim.requestId.originStationId);
      for (int8_t shift = 24; shift >= 0; shift -= 8)
        message.payload[offset++] = static_cast<uint8_t>(claim.requestId.sequence >> shift);
      message.payload[offset++] = static_cast<uint8_t>(claim.requestId.generation >> 8);
      message.payload[offset++] = static_cast<uint8_t>(claim.requestId.generation);
      message.payload[offset++] = static_cast<uint8_t>(claim.cartId >> 8);
      message.payload[offset++] = static_cast<uint8_t>(claim.cartId);
      dependencies_.protocol.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
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
    if (!RequestIdentity::isValid(request.id) ||
        request.destinationId == 0 ||
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
    status = dependencies_.claimClient.onClaimResult(event.payload.claimResult);
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
    dependencies_.diagnostics.record(kDiagnosticProtocolEventRejected,
                                     status.code);
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
