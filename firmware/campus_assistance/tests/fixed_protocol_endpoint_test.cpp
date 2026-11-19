#include "ca/protocol/fixed_protocol_endpoint.hpp"
#include "ca/protocol/fixed_retry_manager.hpp"
#include "ca/storage/fixed_cart_registry.hpp"

#include <assert.h>

using namespace ca;

class FakeRadioTransport : public IRadioTransport {
public:
  FakeRadioTransport() : sentCount(0), receivedHead(0), receivedCount(0) {}

  Status begin(const RadioProfile& profile) override {
    profile_ = profile;
    return Status();
  }

  RadioHealth health() const override {
    RadioHealth result = {true, true, false};
    return result;
  }

  Status send(FrameView frame) override {
    if (frame.data == 0 || frame.length > kMaxFrameBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);
    if (sentCount < 4) {
      sent_[sentCount].length = frame.length;
      for (uint16_t i = 0; i < frame.length; ++i)
        sent_[sentCount].bytes[i] = frame.data[i];
      ++sentCount;
    }
    return Status();
  }

  bool pollReceived(ReceivedFrame& frame) override {
    if (receivedCount == 0)
      return false;
    frame = received_[receivedHead];
    receivedHead = static_cast<uint8_t>((receivedHead + 1U) % 2U);
    --receivedCount;
    return true;
  }

  void service() override {}

  bool inject(const ProtocolMessage& message, const FrameCodec& codec) {
    if (receivedCount >= 2)
      return false;
    uint8_t bytes[kMaxFrameBytes] = {};
    MutableFrameView output = {bytes, kMaxFrameBytes};
    const EncodeResult encoded = codec.encode(message, output);
    if (!encoded.status.ok())
      return false;
    const uint8_t tail = static_cast<uint8_t>((receivedHead + receivedCount) % 2U);
    received_[tail].length = encoded.bytesWritten;
    for (uint16_t i = 0; i < encoded.bytesWritten; ++i)
      received_[tail].bytes[i] = bytes[i];
    ++receivedCount;
    return true;
  }

  uint8_t sentCount;

private:
  RadioProfile profile_;
  ReceivedFrame sent_[4];
  ReceivedFrame received_[2];
  uint8_t receivedHead;
  uint8_t receivedCount;
};

int main() {
  DeviceConfig config = {};
  config.role = DeviceRole::CART;
  config.deviceId = 3;
  config.networkId = 7;
  config.protocolVersion = 1;

  RadioProfile radio = {};
  radio.frequencyHz = 433000000UL;
  radio.spreadingFactor = 9;
  radio.bandwidthHz = 125000UL;
  radio.codingRate = 1;
  radio.transmitPowerDbm = 10;

  BinaryFrameCodec codec;
  DefaultMessageValidator validator;
  FakeRadioTransport transport;
  const RetryPolicy policy = {3, 100, 400};
  FixedRetryManager<4> retries(policy);
  FixedProtocolEndpoint<4> endpoint(
      codec, validator, transport, retries, config, radio);
  assert(endpoint.begin().ok());

  ProtocolEvent claim = {};
  claim.type = ProtocolEventType::CLAIM_RECEIVED;
  claim.envelope.protocolVersion = config.protocolVersion;
  claim.envelope.networkId = config.networkId;
  claim.envelope.sourceDeviceId = config.deviceId;
  claim.envelope.destinationDeviceId = 2;
  claim.payload.claim.requestId = {2, 8, 1};
  claim.payload.claim.cartId = config.deviceId;
  claim.payload.claim.transactionId = 1234;
  ProtocolMessage outgoing = {};
  assert(MessagePayloadCodec::encode(claim, outgoing).ok());
  assert(endpoint.enqueue(outgoing, DeliveryClass::RETRY_UNTIL_ACK).ok());
  endpoint.service(0);
  assert(transport.sentCount == 1);

  ProtocolEvent acknowledgement = {};
  acknowledgement.type = ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED;
  acknowledgement.envelope.protocolVersion = config.protocolVersion;
  acknowledgement.envelope.networkId = config.networkId;
  acknowledgement.envelope.sourceDeviceId = 2;
  acknowledgement.envelope.destinationDeviceId = config.deviceId;
  acknowledgement.envelope.transactionId = 1234;
  ProtocolMessage incoming = {};
  assert(MessagePayloadCodec::encode(acknowledgement, incoming).ok());
  assert(transport.inject(incoming, codec));
  endpoint.service(1);
  RetryOutcome outcome = {};
  assert(endpoint.pollRetryOutcome(outcome));
  assert(outcome.transactionId == 1234);
  assert(outcome.outcome == RetryOutcomeCode::ACKNOWLEDGED);

  ProtocolEvent request = {};
  request.type = ProtocolEventType::REQUEST_RECEIVED;
  request.envelope.protocolVersion = config.protocolVersion;
  request.envelope.networkId = config.networkId;
  request.envelope.sourceDeviceId = 2;
  request.envelope.destinationDeviceId = 0;
  request.envelope.transactionId = 5678;
  request.payload.request.id = {2, 9, 1};
  request.payload.request.destinationId = 4;
  request.payload.request.localAgeMs = 0;
  assert(MessagePayloadCodec::encode(request, incoming).ok());
  assert(transport.inject(incoming, codec));
  endpoint.service(2);

  ProtocolEvent decoded = {};
  assert(endpoint.pollEvent(decoded));
  assert(decoded.type == ProtocolEventType::REQUEST_RECEIVED);
  assert(decoded.senderValidation == SenderValidation::NETWORK_MEMBER_VALIDATED);
  assert(decoded.payload.request.destinationId == 4);
  assert(transport.sentCount == 2); // ACK emitted for the received request.
  assert(!endpoint.pollEvent(decoded));

  // Reliable station broadcasts are expanded to one transaction per cart;
  // an ACK from one cart cannot satisfy another cart's delivery.
  DeviceConfig stationConfig = config;
  stationConfig.role = DeviceRole::STATION;
  stationConfig.deviceId = 1;
  CartRegistryEntry cartEntries[2] = {{4, true}, {5, true}};
  FixedCartRegistry<2> cartRegistry;
  CartRegistryView cartView = {2, cartEntries, 2};
  assert(cartRegistry.load(cartView).ok());

  FakeRadioTransport broadcastTransport;
  FixedRetryManager<1> broadcastRetries(policy);
  FixedProtocolEndpoint<4> broadcastEndpoint(codec,
                                             validator,
                                             broadcastTransport,
                                             broadcastRetries,
                                             stationConfig,
                                             radio,
                                             cartRegistry);
  assert(broadcastEndpoint.begin().ok());

  ProtocolEvent assignment = {};
  assignment.type = ProtocolEventType::ASSIGNMENT_RECEIVED;
  assignment.envelope.protocolVersion = stationConfig.protocolVersion;
  assignment.envelope.networkId = stationConfig.networkId;
  assignment.envelope.sourceDeviceId = stationConfig.deviceId;
  assignment.envelope.destinationDeviceId = 0;
  assignment.envelope.transactionId = 500;
  assignment.payload.assignment.assignment.requestId = {1, 11, 1};
  assignment.payload.assignment.assignment.assignedCartId = 4;
  assignment.payload.assignment.assignment.epoch = 1;
  assignment.payload.assignment.assignment.state = RequestState::ASSIGNED;
  assert(MessagePayloadCodec::encode(assignment, outgoing).ok());
  assert(broadcastEndpoint.enqueue(outgoing, DeliveryClass::RETRY_UNTIL_ACK).ok());
  broadcastEndpoint.service(0);
  assert(broadcastTransport.sentCount == 1);

  ProtocolEvent wrongAck = {};
  wrongAck.type = ProtocolEventType::ACKNOWLEDGEMENT_RECEIVED;
  wrongAck.envelope.protocolVersion = stationConfig.protocolVersion;
  wrongAck.envelope.networkId = stationConfig.networkId;
  wrongAck.envelope.sourceDeviceId = 4;
  wrongAck.envelope.destinationDeviceId = stationConfig.deviceId;
  wrongAck.envelope.transactionId = 500 + 5UL * 65537UL;
  assert(MessagePayloadCodec::encode(wrongAck, incoming).ok());
  assert(broadcastTransport.inject(incoming, codec));
  broadcastEndpoint.service(1);
  assert(!broadcastEndpoint.pollRetryOutcome(outcome));

  wrongAck.envelope.transactionId = 500 + 4UL * 65537UL;
  assert(MessagePayloadCodec::encode(wrongAck, incoming).ok());
  assert(broadcastTransport.inject(incoming, codec));
  broadcastEndpoint.service(2);
  assert(broadcastEndpoint.pollRetryOutcome(outcome));
  assert(outcome.transactionId == 500 + 4UL * 65537UL);
  assert(outcome.outcome == RetryOutcomeCode::ACKNOWLEDGED);

  broadcastEndpoint.service(3);
  assert(broadcastTransport.sentCount == 2);
  wrongAck.envelope.sourceDeviceId = 5;
  wrongAck.envelope.transactionId = 500 + 5UL * 65537UL;
  assert(MessagePayloadCodec::encode(wrongAck, incoming).ok());
  assert(broadcastTransport.inject(incoming, codec));
  broadcastEndpoint.service(4);
  assert(broadcastEndpoint.pollRetryOutcome(outcome));
  assert(outcome.transactionId == 500 + 5UL * 65537UL);
  assert(outcome.outcome == RetryOutcomeCode::ACKNOWLEDGED);
  return 0;
}
