#include "ca/ui/basic_user_interfaces.hpp"
#include "ca/storage/fixed_destination_catalog.hpp"

#include <assert.h>
#include <string.h>

using namespace ca;

class FakeLifecycle : public RequestLifecycle {
public:
  FakeLifecycle() : nextSequence_(1), cancelCount_(0) {}
  Status configure() override { return Status(); }
  Status create(const NewRequest& input, RequestRecord& record) override {
    record = {};
    record.id = {1, nextSequence_++, 1};
    record.destinationId = input.destinationId;
    record.state = RequestState::PENDING_UNASSIGNED;
    return Status();
  }
  Status cancel(const RequestId&, CancellationSource) override {
    ++cancelCount_;
    return Status();
  }
  Status expireDue(uint32_t, ExpiredRequestSet&) override { return Status(); }
  Status applyJourneyUpdate(const JourneyUpdate&) override { return Status(); }
  bool find(const RequestId&, RequestRecord&) const override { return false; }

private:
  uint32_t nextSequence_;

public:
  uint16_t cancelCount_;
};

class FakeProtocol : public ProtocolEndpoint {
public:
  FakeProtocol() : enqueueCount_(0) {}
  Status begin() override { return Status(); }
  void service(uint32_t) override {}
  Status enqueue(const ProtocolMessage&, DeliveryClass) override {
    ++enqueueCount_;
    return Status();
  }
  bool pollEvent(ProtocolEvent&) override { return false; }
  bool pollRetryOutcome(RetryOutcome&) override { return false; }
  uint16_t enqueueCount_;
};

int main() {
  BasicCartUserInterface cartUi;
  CartUiContext context = {};
  context.acceptanceMode = AcceptanceMode::NOT_ACCEPTING;
  context.serviceState = CartServiceState::IDLE;
  cartUi.update(context);
  UiViewModel view = cartUi.view();
  assert(strncmp(view.line1, "Accept: OFF", 11) == 0);
  assert(strncmp(view.line2, "Ready / Claim", 13) == 0);

  context.acceptanceMode = AcceptanceMode::ACCEPTING;
  context.serviceState = CartServiceState::CLAIM_PENDING;
  cartUi.update(context);
  view = cartUi.view();
  assert(strncmp(view.line1, "Accept: ON", 10) == 0);
  assert(strncmp(view.line2, "Claim pending", 13) == 0);

  context.serviceState = CartServiceState::SERVING;
  context.activeAssignment = true;
  cartUi.update(context);
  view = cartUi.view();
  assert(strncmp(view.line2, "Serving / Hold B", 16) == 0);

  context.serviceState = CartServiceState::UNCERTAIN;
  cartUi.update(context);
  view = cartUi.view();
  assert(strncmp(view.line2, "Check station", 13) == 0);

  Destination entries[2] = {};
  entries[0].id = 3;
  entries[0].enabled = true;
  strcpy(entries[0].label, "Library");
  entries[1].id = 4;
  entries[1].enabled = true;
  strcpy(entries[1].label, "Canteen");
  FixedDestinationCatalog<2> destinations;
  assert(destinations.load({2, entries, 2}).ok());

  FakeLifecycle lifecycle;
  FakeProtocol protocol;
  DeviceConfig config = {};
  config.role = DeviceRole::STATION;
  config.deviceId = 1;
  config.networkId = 7;
  config.protocolVersion = 1;
  StationRequestController requests(destinations, lifecycle, protocol, config);
  BasicStationUserInterface stationUi(destinations, requests);

  view = stationUi.view();
  assert(strncmp(view.line1, "> Library", 9) == 0);
  UiIntent next = {UiIntentType::NEXT, 0};
  stationUi.onIntent(next);
  view = stationUi.view();
  assert(strncmp(view.line1, "> Canteen", 9) == 0);

  UiIntent confirm = {UiIntentType::CONFIRM, 0};
  stationUi.onIntent(confirm);
  assert(requests.hasCurrentRequest());
  view = stationUi.view();
  assert(strncmp(view.line2, "Request active", 14) == 0);

  UiIntent cancel = {UiIntentType::CANCEL, 0};
  stationUi.onIntent(cancel);
  assert(!requests.hasCurrentRequest());
  assert(lifecycle.cancelCount_ == 1);
  assert(protocol.enqueueCount_ == 2);
  view = stationUi.view();
  assert(strncmp(view.line2, "Cancel queued", 13) == 0);
  return 0;
}
