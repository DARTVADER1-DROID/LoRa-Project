#include "CampusAssistance.h"
#include "ca/storage/fixed_destination_catalog.hpp"

#include <assert.h>

using namespace ca;

class FakeLifecycle : public RequestLifecycle {
public:
  FakeLifecycle()
      : next_(1), cancelCount_(0), lastSource_(CancellationSource::SYSTEM) {}
  Status configure() override { return Status(); }
  Status create(const NewRequest& input, RequestRecord& created) override {
    created = {};
    created.id = {1, next_++, 1};
    created.destinationId = input.destinationId;
    created.createdAtMs = 500;
    created.expiresAtMs = 600500;
    created.state = RequestState::PENDING_UNASSIGNED;
    created.claimState = ClaimState::IDLE;
    return Status();
  }
  Status cancel(const RequestId&, CancellationSource source) override {
    ++cancelCount_;
    lastSource_ = source;
    return Status();
  }
  Status expireDue(uint32_t, ExpiredRequestSet&) override { return Status(); }
  Status applyJourneyUpdate(const JourneyUpdate&) override { return Status(); }
  bool find(const RequestId&, RequestRecord&) const override { return false; }

  uint16_t cancelCount_;
  CancellationSource lastSource_;

private:
  uint32_t next_;
};

class FakeProtocol : public ProtocolEndpoint {
public:
  FakeProtocol() : enqueueStatus_(), enqueued_(false) {}
  Status begin() override { return Status(); }
  void service(uint32_t) override {}
  Status enqueue(const ProtocolMessage& message, DeliveryClass delivery) override {
    lastMessage_ = message;
    lastDelivery_ = delivery;
    enqueued_ = true;
    return enqueueStatus_;
  }
  bool pollEvent(ProtocolEvent&) override { return false; }

  ProtocolMessage lastMessage_;
  DeliveryClass lastDelivery_;
  Status enqueueStatus_;
  bool enqueued_;
};

int main() {
  Destination entries[1] = {};
  entries[0].id = 3;
  entries[0].enabled = true;
  entries[0].label[0] = 'L';
  entries[0].label[1] = '\0';
  FixedDestinationCatalog<2> destinations;
  assert(destinations.load({1, entries, 1}).ok());

  FakeLifecycle lifecycle;
  FakeProtocol protocol;
  DeviceConfig config = {};
  config.role = DeviceRole::STATION;
  config.deviceId = 1;
  config.networkId = 7;
  config.protocolVersion = 1;
  StationRequestController controller(destinations, lifecycle, protocol, config);

  assert(controller.selectDestination(99).code == ErrorCode::NOT_FOUND);
  assert(controller.selectDestination(3).ok());
  assert(!controller.hasCurrentRequest());
  RequestId created = {};
  assert(controller.submit(created).ok());
  assert(created.sequence == 1);
  assert(controller.hasCurrentRequest());
  assert(protocol.enqueued_);
  assert(protocol.lastDelivery_ == DeliveryClass::RETRY_UNTIL_ACK);
  assert(protocol.lastMessage_.header.messageType == MessageType::REQUEST);
  assert(protocol.lastMessage_.payloadLength == 14);
  const TransactionId requestTransaction = protocol.lastMessage_.header.transactionId;
  assert(controller.submit(created).code == ErrorCode::BUSY);
  protocol.enqueueStatus_ = Status(ErrorCode::QUEUE_FULL);
  assert(controller.cancelCurrentRequest().code == ErrorCode::QUEUE_FULL);
  assert(protocol.lastMessage_.header.messageType == MessageType::CANCEL);
  assert(protocol.lastMessage_.payloadLength == 9);
  assert(protocol.lastMessage_.header.transactionId != requestTransaction);
  assert(lifecycle.cancelCount_ == 1);
  assert(controller.hasCurrentRequest());
  assert(lifecycle.lastSource_ == CancellationSource::STUDENT);
  assert(controller.submit(created).code == ErrorCode::BUSY);

  protocol.enqueueStatus_ = Status();
  assert(controller.service().ok());
  assert(!controller.hasCurrentRequest());
  assert(lifecycle.cancelCount_ == 1);

  protocol.enqueueStatus_ = Status(ErrorCode::QUEUE_FULL);
  assert(controller.submit(created).code == ErrorCode::QUEUE_FULL);
  assert(lifecycle.cancelCount_ == 2);
  assert(lifecycle.lastSource_ == CancellationSource::SYSTEM);
  return 0;
}
