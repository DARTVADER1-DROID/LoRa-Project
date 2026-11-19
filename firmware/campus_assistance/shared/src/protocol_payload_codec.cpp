#include "ca/protocol.hpp"
#include "ca/domain/request_identity.hpp"

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

uint16_t get16(const uint8_t* input, uint16_t& offset) {
  const uint16_t high = input[offset++];
  return static_cast<uint16_t>((high << 8) | input[offset++]);
}

uint32_t get32(const uint8_t* input, uint16_t& offset) {
  const uint32_t b0 = input[offset++];
  const uint32_t b1 = input[offset++];
  const uint32_t b2 = input[offset++];
  const uint32_t b3 = input[offset++];
  return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}

void putRequestId(uint8_t* output, uint16_t& offset, const RequestId& id) {
  put16(output, offset, id.originStationId);
  put32(output, offset, id.sequence);
  put16(output, offset, id.generation);
}

RequestId getRequestId(const uint8_t* input, uint16_t& offset) {
  RequestId id = {};
  id.originStationId = get16(input, offset);
  id.sequence = get32(input, offset);
  id.generation = get16(input, offset);
  return id;
}

bool validError(ErrorCode value) {
  return static_cast<uint8_t>(value) <=
         static_cast<uint8_t>(ErrorCode::NOT_IMPLEMENTED);
}

bool validClaimState(ClaimState value) {
  return static_cast<uint8_t>(value) <= static_cast<uint8_t>(ClaimState::COMMITTED);
}

bool validRequestState(RequestState value) {
  return static_cast<uint8_t>(value) <=
         static_cast<uint8_t>(RequestState::COMMUNICATION_UNCERTAIN);
}

bool validSnapshot(const RequestSnapshot& snapshot) {
  if (!RequestIdentity::isValid(snapshot.requestId) ||
      !validRequestState(snapshot.state) || !validClaimState(snapshot.claimState))
    return false;

  const bool assigned = snapshot.state == RequestState::ASSIGNED ||
                        snapshot.state == RequestState::ON_THE_WAY ||
                        snapshot.state == RequestState::ARRIVED_AT_PICKUP ||
                        snapshot.state == RequestState::COMPLETED;
  if (assigned)
    return snapshot.assignedCartId != 0 && snapshot.assignmentEpoch != 0 &&
           snapshot.claimState == ClaimState::COMMITTED &&
           snapshot.claimCartId == snapshot.assignedCartId &&
           snapshot.claimTransactionId != 0 &&
           (snapshot.state == RequestState::ASSIGNED ? snapshot.lastEventSequence == 0
                                                     : snapshot.lastEventSequence != 0);

  if (snapshot.assignedCartId != 0 || snapshot.assignmentEpoch != 0 ||
      snapshot.lastEventSequence != 0)
    return false;
  if (snapshot.state == RequestState::CLAIM_PENDING)
    return snapshot.claimState == ClaimState::PENDING && snapshot.claimCartId != 0 &&
           snapshot.claimTransactionId != 0;
  if (snapshot.claimState == ClaimState::IDLE)
    return snapshot.claimCartId == 0 && snapshot.claimTransactionId == 0;
  if (snapshot.claimState == ClaimState::WITHDRAWN)
    return snapshot.claimCartId != 0 && snapshot.claimTransactionId != 0;
  return false;
}

bool validJourneyStatus(JourneyStatus value) {
  return static_cast<uint8_t>(value) <= static_cast<uint8_t>(JourneyStatus::COMPLETED);
}

bool validCancellationSource(CancellationSource value) {
  return static_cast<uint8_t>(value) <=
         static_cast<uint8_t>(CancellationSource::SYSTEM);
}

ProtocolEventType eventTypeFor(MessageType type) {
  switch (type) {
  case MessageType::REQUEST:
    return ProtocolEventType::REQUEST_RECEIVED;
  case MessageType::CLAIM:
    return ProtocolEventType::CLAIM_RECEIVED;
  case MessageType::CLAIM_WITHDRAW:
    return ProtocolEventType::CLAIM_WITHDRAW_RECEIVED;
  case MessageType::CLAIM_RESULT:
    return ProtocolEventType::CLAIM_RESULT_RECEIVED;
  case MessageType::ASSIGN:
    return ProtocolEventType::ASSIGNMENT_RECEIVED;
  case MessageType::CANCEL:
    return ProtocolEventType::CANCELLATION_RECEIVED;
  case MessageType::EXPIRED:
    return ProtocolEventType::EXPIRY_RECEIVED;
  case MessageType::JOURNEY_UPDATE:
    return ProtocolEventType::JOURNEY_UPDATE_RECEIVED;
  case MessageType::SNAPSHOT_QUERY:
    return ProtocolEventType::SNAPSHOT_QUERY_RECEIVED;
  case MessageType::SNAPSHOT:
    return ProtocolEventType::SNAPSHOT_RECEIVED;
  case MessageType::ACKNOWLEDGEMENT:
    return ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED;
  }
  return ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED;
}

MessageType messageTypeFor(ProtocolEventType type) {
  switch (type) {
  case ProtocolEventType::REQUEST_RECEIVED:
    return MessageType::REQUEST;
  case ProtocolEventType::CLAIM_RECEIVED:
    return MessageType::CLAIM;
  case ProtocolEventType::CLAIM_WITHDRAW_RECEIVED:
    return MessageType::CLAIM_WITHDRAW;
  case ProtocolEventType::CLAIM_RESULT_RECEIVED:
    return MessageType::CLAIM_RESULT;
  case ProtocolEventType::ASSIGNMENT_RECEIVED:
    return MessageType::ASSIGN;
  case ProtocolEventType::CANCELLATION_RECEIVED:
    return MessageType::CANCEL;
  case ProtocolEventType::EXPIRY_RECEIVED:
    return MessageType::EXPIRED;
  case ProtocolEventType::JOURNEY_UPDATE_RECEIVED:
    return MessageType::JOURNEY_UPDATE;
  case ProtocolEventType::SNAPSHOT_QUERY_RECEIVED:
    return MessageType::SNAPSHOT_QUERY;
  case ProtocolEventType::SNAPSHOT_RECEIVED:
    return MessageType::SNAPSHOT;
  case ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED:
    return MessageType::ACKNOWLEDGEMENT;
  }
  return MessageType::ACKNOWLEDGEMENT;
}

} // namespace

Status MessagePayloadCodec::decode(const ProtocolMessage& message,
                                   ProtocolEvent& event) {
  event = ProtocolEvent();
  event.type = eventTypeFor(message.header.messageType);
  event.envelope = message.header;
  event.senderValidation = SenderValidation::UNVALIDATED;
  const uint8_t* input = message.payload;
  uint16_t offset = 0;

  switch (message.header.messageType) {
  case MessageType::REQUEST:
    if (message.payloadLength != 14)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.request.id = getRequestId(input, offset);
    event.payload.request.destinationId = get16(input, offset);
    event.payload.request.localAgeMs = get32(input, offset);
    if (!RequestIdentity::isValid(event.payload.request.id) ||
        event.payload.request.destinationId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::CLAIM:
    if (message.payloadLength != 10)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.claim.requestId = getRequestId(input, offset);
    event.payload.claim.cartId = get16(input, offset);
    event.payload.claim.transactionId = message.header.transactionId;
    if (!RequestIdentity::isValid(event.payload.claim.requestId) ||
        event.payload.claim.cartId == 0 || message.header.transactionId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::CLAIM_WITHDRAW:
    if (message.payloadLength != 14 || message.header.transactionId == 0)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.claimWithdraw.requestId = getRequestId(input, offset);
    event.payload.claimWithdraw.cartId = get16(input, offset);
    event.payload.claimWithdraw.transactionId = get32(input, offset);
    if (!RequestIdentity::isValid(event.payload.claimWithdraw.requestId) ||
        event.payload.claimWithdraw.cartId == 0 ||
        event.payload.claimWithdraw.transactionId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::CLAIM_RESULT:
    if (message.payloadLength != 10)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.claimResult.requestId = getRequestId(input, offset);
    event.payload.claimResult.transactionId = message.header.transactionId;
    event.payload.claimResult.result = static_cast<ErrorCode>(input[offset++]);
    event.payload.claimResult.finalState = static_cast<ClaimState>(input[offset++]);
    if (!RequestIdentity::isValid(event.payload.claimResult.requestId) ||
        message.header.transactionId == 0 ||
        !validError(event.payload.claimResult.result) ||
        !validClaimState(event.payload.claimResult.finalState))
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::ASSIGN:
    if (message.payloadLength != 13)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.assignment.assignment.requestId = getRequestId(input, offset);
    event.payload.assignment.assignment.assignedCartId = get16(input, offset);
    event.payload.assignment.assignment.epoch = get16(input, offset);
    event.payload.assignment.assignment.state =
        static_cast<RequestState>(input[offset++]);
    event.payload.assignment.assignment.lastEventSequence = 0;
    if (!RequestIdentity::isValid(event.payload.assignment.assignment.requestId) ||
        event.payload.assignment.assignment.assignedCartId == 0 ||
        event.payload.assignment.assignment.epoch == 0 ||
        event.payload.assignment.assignment.state != RequestState::ASSIGNED)
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::CANCEL:
    if (message.payloadLength != 9)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.cancellation.requestId = getRequestId(input, offset);
    event.payload.cancellation.source =
        static_cast<CancellationSource>(input[offset++]);
    if (!RequestIdentity::isValid(event.payload.cancellation.requestId) ||
        !validCancellationSource(event.payload.cancellation.source))
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::EXPIRED:
    if (message.payloadLength != 8)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.expiry.requestId = getRequestId(input, offset);
    return RequestIdentity::isValid(event.payload.expiry.requestId)
               ? Status()
               : Status(ErrorCode::INVALID_ARGUMENT);

  case MessageType::JOURNEY_UPDATE:
    if (message.payloadLength != 15)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.journeyUpdate.requestId = getRequestId(input, offset);
    event.payload.journeyUpdate.assignedCartId = get16(input, offset);
    event.payload.journeyUpdate.status = static_cast<JourneyStatus>(input[offset++]);
    event.payload.journeyUpdate.eventSequence = get32(input, offset);
    if (!RequestIdentity::isValid(event.payload.journeyUpdate.requestId) ||
        event.payload.journeyUpdate.assignedCartId == 0 ||
        !validJourneyStatus(event.payload.journeyUpdate.status))
      return Status(ErrorCode::INVALID_ARGUMENT);
    return Status();

  case MessageType::SNAPSHOT_QUERY:
    if (message.payloadLength != 8)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.snapshotQuery = getRequestId(input, offset);
    return RequestIdentity::isValid(event.payload.snapshotQuery)
               ? Status()
               : Status(ErrorCode::INVALID_ARGUMENT);

  case MessageType::SNAPSHOT:
    if (message.payloadLength != 24)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.snapshot.requestId = getRequestId(input, offset);
    event.payload.snapshot.state = static_cast<RequestState>(input[offset++]);
    event.payload.snapshot.assignedCartId = get16(input, offset);
    event.payload.snapshot.assignmentEpoch = get16(input, offset);
    event.payload.snapshot.lastEventSequence = get32(input, offset);
    event.payload.snapshot.claimState = static_cast<ClaimState>(input[offset++]);
    event.payload.snapshot.claimCartId = get16(input, offset);
    event.payload.snapshot.claimTransactionId = get32(input, offset);
    return validSnapshot(event.payload.snapshot) ? Status()
                                                 : Status(ErrorCode::INVALID_ARGUMENT);

  case MessageType::ACKNOWLEDGEMENT:
    if (message.payloadLength != 0)
      return Status(ErrorCode::PROTOCOL_MISMATCH);
    event.payload.acknowledgedTransaction = message.header.transactionId;
    return Status();
  }
  return Status(ErrorCode::PROTOCOL_MISMATCH);
}

Status MessagePayloadCodec::encode(const ProtocolEvent& event,
                                   ProtocolMessage& message) {
  message = ProtocolMessage();
  message.header = event.envelope;
  message.header.messageType = messageTypeFor(event.type);
  uint16_t offset = 0;

  switch (event.type) {
  case ProtocolEventType::REQUEST_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.request.id) ||
        event.payload.request.destinationId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.request.id);
    put16(message.payload, offset, event.payload.request.destinationId);
    put32(message.payload, offset, event.payload.request.localAgeMs);
    break;

  case ProtocolEventType::CLAIM_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.claim.requestId) ||
        event.payload.claim.cartId == 0 || event.payload.claim.transactionId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    message.header.transactionId = event.payload.claim.transactionId;
    putRequestId(message.payload, offset, event.payload.claim.requestId);
    put16(message.payload, offset, event.payload.claim.cartId);
    break;

  case ProtocolEventType::CLAIM_WITHDRAW_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.claimWithdraw.requestId) ||
        event.payload.claimWithdraw.cartId == 0 ||
        event.payload.claimWithdraw.transactionId == 0 ||
        message.header.transactionId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.claimWithdraw.requestId);
    put16(message.payload, offset, event.payload.claimWithdraw.cartId);
    put32(message.payload, offset, event.payload.claimWithdraw.transactionId);
    break;

  case ProtocolEventType::CLAIM_RESULT_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.claimResult.requestId) ||
        event.payload.claimResult.transactionId == 0 ||
        !validError(event.payload.claimResult.result) ||
        !validClaimState(event.payload.claimResult.finalState))
      return Status(ErrorCode::INVALID_ARGUMENT);
    message.header.transactionId = event.payload.claimResult.transactionId;
    putRequestId(message.payload, offset, event.payload.claimResult.requestId);
    message.payload[offset++] = static_cast<uint8_t>(event.payload.claimResult.result);
    message.payload[offset++] =
        static_cast<uint8_t>(event.payload.claimResult.finalState);
    break;

  case ProtocolEventType::ASSIGNMENT_RECEIVED: {
    const AssignmentRecord& assignment = event.payload.assignment.assignment;
    if (!RequestIdentity::isValid(assignment.requestId) ||
        assignment.assignedCartId == 0 || assignment.epoch == 0 ||
        assignment.state != RequestState::ASSIGNED)
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, assignment.requestId);
    put16(message.payload, offset, assignment.assignedCartId);
    put16(message.payload, offset, assignment.epoch);
    message.payload[offset++] = static_cast<uint8_t>(RequestState::ASSIGNED);
    break;
  }

  case ProtocolEventType::CANCELLATION_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.cancellation.requestId) ||
        !validCancellationSource(event.payload.cancellation.source))
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.cancellation.requestId);
    message.payload[offset++] = static_cast<uint8_t>(event.payload.cancellation.source);
    break;

  case ProtocolEventType::EXPIRY_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.expiry.requestId))
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.expiry.requestId);
    break;

  case ProtocolEventType::JOURNEY_UPDATE_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.journeyUpdate.requestId) ||
        event.payload.journeyUpdate.assignedCartId == 0 ||
        !validJourneyStatus(event.payload.journeyUpdate.status))
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.journeyUpdate.requestId);
    put16(message.payload, offset, event.payload.journeyUpdate.assignedCartId);
    message.payload[offset++] =
        static_cast<uint8_t>(event.payload.journeyUpdate.status);
    put32(message.payload, offset, event.payload.journeyUpdate.eventSequence);
    break;

  case ProtocolEventType::SNAPSHOT_QUERY_RECEIVED:
    if (!RequestIdentity::isValid(event.payload.snapshotQuery))
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.snapshotQuery);
    break;

  case ProtocolEventType::SNAPSHOT_RECEIVED:
    if (!validSnapshot(event.payload.snapshot))
      return Status(ErrorCode::INVALID_ARGUMENT);
    putRequestId(message.payload, offset, event.payload.snapshot.requestId);
    message.payload[offset++] = static_cast<uint8_t>(event.payload.snapshot.state);
    put16(message.payload, offset, event.payload.snapshot.assignedCartId);
    put16(message.payload, offset, event.payload.snapshot.assignmentEpoch);
    put32(message.payload, offset, event.payload.snapshot.lastEventSequence);
    message.payload[offset++] = static_cast<uint8_t>(event.payload.snapshot.claimState);
    put16(message.payload, offset, event.payload.snapshot.claimCartId);
    put32(message.payload, offset, event.payload.snapshot.claimTransactionId);
    break;

  case ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED:
    offset = 0;
    break;
  }

  if (offset > kMaxProtocolPayloadBytes)
    return Status(ErrorCode::BUFFER_FULL);
  message.payloadLength = offset;
  return Status();
}

} // namespace ca
