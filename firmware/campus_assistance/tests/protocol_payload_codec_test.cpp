#include "CampusAssistance.h"

#include <assert.h>

using namespace ca;

static ProtocolEvent baseEvent(ProtocolEventType type, MessageType messageType) {
  ProtocolEvent event = {};
  event.type = type;
  event.envelope.protocolVersion = 1;
  event.envelope.networkId = 7;
  event.envelope.sourceDeviceId = 1;
  event.envelope.destinationDeviceId = 0;
  event.envelope.messageType = messageType;
  event.senderValidation = SenderValidation::NETWORK_MEMBER_VALIDATED;
  return event;
}

static ProtocolEvent roundTrip(const ProtocolEvent& source) {
  ProtocolMessage encoded = {};
  assert(MessagePayloadCodec::encode(source, encoded).ok());
  ProtocolEvent decoded = {};
  assert(MessagePayloadCodec::decode(encoded, decoded).ok());
  assert(decoded.type == source.type);
  assert(decoded.envelope.messageType == source.envelope.messageType);
  assert(decoded.envelope.sourceDeviceId == source.envelope.sourceDeviceId);
  return decoded;
}

int main() {
  const RequestId id = {1, 0x12345678UL, 2};

  ProtocolEvent request =
      baseEvent(ProtocolEventType::REQUEST_RECEIVED, MessageType::REQUEST);
  request.payload.request.id = id;
  request.payload.request.destinationId = 3;
  request.payload.request.localAgeMs = 456;
  ProtocolEvent decoded = roundTrip(request);
  assert(RequestIdentity::equals(decoded.payload.request.id, id));
  assert(decoded.payload.request.destinationId == 3);
  assert(decoded.payload.request.localAgeMs == 456);

  ProtocolEvent claim =
      baseEvent(ProtocolEventType::CLAIM_RECEIVED, MessageType::CLAIM);
  claim.payload.claim.requestId = id;
  claim.payload.claim.cartId = 8;
  claim.payload.claim.transactionId = 99;
  decoded = roundTrip(claim);
  assert(decoded.payload.claim.cartId == 8);
  assert(decoded.payload.claim.transactionId == 99);

  ProtocolEvent withdraw = baseEvent(ProtocolEventType::CLAIM_WITHDRAW_RECEIVED,
                                     MessageType::CLAIM_WITHDRAW);
  withdraw.payload.claimWithdraw.requestId = id;
  withdraw.payload.claimWithdraw.cartId = 8;
  withdraw.payload.claimWithdraw.transactionId = 100;
  decoded = roundTrip(withdraw);
  assert(decoded.payload.claimWithdraw.transactionId == 100);

  ProtocolEvent result =
      baseEvent(ProtocolEventType::CLAIM_RESULT_RECEIVED, MessageType::CLAIM_RESULT);
  result.payload.claimResult.requestId = id;
  result.payload.claimResult.transactionId = 101;
  result.payload.claimResult.result = ErrorCode::NONE;
  result.payload.claimResult.finalState = ClaimState::COMMITTED;
  decoded = roundTrip(result);
  assert(decoded.payload.claimResult.finalState == ClaimState::COMMITTED);

  ProtocolEvent assignment =
      baseEvent(ProtocolEventType::ASSIGNMENT_RECEIVED, MessageType::ASSIGN);
  assignment.payload.assignment.assignment.requestId = id;
  assignment.payload.assignment.assignment.assignedCartId = 8;
  assignment.payload.assignment.assignment.epoch = 1;
  assignment.payload.assignment.assignment.state = RequestState::ASSIGNED;
  decoded = roundTrip(assignment);
  assert(decoded.payload.assignment.assignment.assignedCartId == 8);
  assert(decoded.payload.assignment.assignment.lastEventSequence == 0);

  ProtocolEvent cancellation =
      baseEvent(ProtocolEventType::CANCELLATION_RECEIVED, MessageType::CANCEL);
  cancellation.payload.cancellation.requestId = id;
  cancellation.payload.cancellation.source = CancellationSource::STUDENT;
  decoded = roundTrip(cancellation);
  assert(decoded.payload.cancellation.source == CancellationSource::STUDENT);

  ProtocolEvent expiry =
      baseEvent(ProtocolEventType::EXPIRY_RECEIVED, MessageType::EXPIRED);
  expiry.payload.expiry.requestId = id;
  decoded = roundTrip(expiry);
  assert(RequestIdentity::equals(decoded.payload.expiry.requestId, id));

  ProtocolEvent journey = baseEvent(ProtocolEventType::JOURNEY_UPDATE_RECEIVED,
                                    MessageType::JOURNEY_UPDATE);
  journey.payload.journeyUpdate.requestId = id;
  journey.payload.journeyUpdate.assignedCartId = 8;
  journey.payload.journeyUpdate.status = JourneyStatus::ON_THE_WAY;
  journey.payload.journeyUpdate.eventSequence = 7;
  decoded = roundTrip(journey);
  assert(decoded.payload.journeyUpdate.eventSequence == 7);

  ProtocolEvent query = baseEvent(ProtocolEventType::SNAPSHOT_QUERY_RECEIVED,
                                  MessageType::SNAPSHOT_QUERY);
  query.payload.snapshotQuery = id;
  decoded = roundTrip(query);
  assert(RequestIdentity::equals(decoded.payload.snapshotQuery, id));

  ProtocolEvent snapshot =
      baseEvent(ProtocolEventType::SNAPSHOT_RECEIVED, MessageType::SNAPSHOT);
  snapshot.payload.snapshot.requestId = id;
  snapshot.payload.snapshot.state = RequestState::ON_THE_WAY;
  snapshot.payload.snapshot.assignedCartId = 8;
  snapshot.payload.snapshot.assignmentEpoch = 1;
  snapshot.payload.snapshot.lastEventSequence = 7;
  decoded = roundTrip(snapshot);
  assert(decoded.payload.snapshot.lastEventSequence == 7);

  ProtocolEvent acknowledgement = baseEvent(ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED,
                                            MessageType::ACKNOWLEDGEMENT);
  acknowledgement.envelope.transactionId = 123;
  decoded = roundTrip(acknowledgement);
  assert(decoded.payload.acknowledgedTransaction == 123);

  ProtocolMessage malformed = {};
  malformed.header.messageType = MessageType::CLAIM;
  malformed.payloadLength = 9;
  assert(MessagePayloadCodec::decode(malformed, decoded).code ==
         ErrorCode::PROTOCOL_MISMATCH);
  return 0;
}
