#include "ca/station_request_controller.hpp"

namespace ca {
namespace {

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
} // namespace

StationRequestController::StationRequestController(DestinationCatalog& destinations,
                                                   RequestLifecycle& lifecycle,
                                                   IClock& clock,
                                                   ProtocolEndpoint& protocol,
                                                   const DeviceConfig& config)
    : destinations_(destinations), lifecycle_(lifecycle), clock_(clock),
      protocol_(protocol), config_(config), selectedDestinationId_(0),
      currentRequestId_(), hasSelection_(false), hasCurrentRequest_(false) {
}

Status StationRequestController::selectDestination(DeviceId destinationId) {
  if (destinationId == 0)
    return Status(ErrorCode::INVALID_ARGUMENT);
  Destination destination = {};
  if (!destinations_.findById(destinationId, destination) || !destination.enabled)
    return Status(ErrorCode::NOT_FOUND);
  selectedDestinationId_ = destinationId;
  hasSelection_ = true;
  return Status();
}

Status StationRequestController::submit(RequestId& createdRequestId) {
  if (hasCurrentRequest_)
    return Status(ErrorCode::BUSY);
  if (!hasSelection_)
    return Status(ErrorCode::INVALID_STATE);
  if (config_.role != DeviceRole::STATION || config_.deviceId == 0 ||
      config_.networkId == 0 || config_.protocolVersion == 0)
    return Status(ErrorCode::CONFIGURATION_INVALID);

  NewRequest input = {};
  input.destinationId = selectedDestinationId_;
  RequestRecord created = {};
  const Status creation = lifecycle_.create(input, created);
  if (!creation.ok())
    return creation;

  ProtocolMessage message = {};
  message.header.protocolVersion = config_.protocolVersion;
  message.header.networkId = config_.networkId;
  message.header.messageType = MessageType::REQUEST;
  message.header.sourceDeviceId = config_.deviceId;
  message.header.destinationDeviceId = 0;
  message.header.transactionId = transactionFor(created.id);
  message.header.messageSequence = created.id.sequence;
  message.payloadLength = 14;
  uint16_t offset = 0;
  put16(message.payload, offset, created.id.originStationId);
  put32(message.payload, offset, created.id.sequence);
  put16(message.payload, offset, created.id.generation);
  put16(message.payload, offset, created.destinationId);
  put32(message.payload, offset, 0);

  const Status queued = protocol_.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
  if (!queued.ok()) {
    lifecycle_.cancel(created.id, CancellationSource::SYSTEM);
    return queued;
  }

  currentRequestId_ = created.id;
  hasCurrentRequest_ = true;
  createdRequestId = created.id;
  return Status();
}

Status StationRequestController::cancelCurrentRequest() {
  if (!hasCurrentRequest_)
    return Status(ErrorCode::NOT_FOUND);

  const RequestId id = currentRequestId_;
  const Status cancelled = lifecycle_.cancel(id, CancellationSource::STUDENT);
  if (!cancelled.ok())
    return cancelled;

  ProtocolMessage message = {};
  message.header.protocolVersion = config_.protocolVersion;
  message.header.networkId = config_.networkId;
  message.header.messageType = MessageType::CANCEL;
  message.header.sourceDeviceId = config_.deviceId;
  message.header.destinationDeviceId = 0;
  message.header.transactionId = transactionFor(id);
  message.header.messageSequence = id.sequence;
  message.payloadLength = 9;
  uint16_t offset = 0;
  put16(message.payload, offset, id.originStationId);
  put32(message.payload, offset, id.sequence);
  put16(message.payload, offset, id.generation);
  message.payload[offset++] = static_cast<uint8_t>(CancellationSource::STUDENT);

  const Status queued = protocol_.enqueue(message, DeliveryClass::RETRY_UNTIL_ACK);
  hasCurrentRequest_ = false;
  currentRequestId_ = RequestId();
  return queued;
}

} // namespace ca
