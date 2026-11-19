#include "CampusAssistance.h"

#include <assert.h>

using namespace ca;

class FakeClock : public IClock {
public:
  FakeClock() : now_(100) {}
  uint32_t millisNow() const override { return now_; }
  void set(uint32_t now) { now_ = now; }

private:
  uint32_t now_;
};

class FakeDestinationCatalog : public DestinationCatalog {
public:
  FakeDestinationCatalog() : enabled_(true) {}

  Status load(const DestinationConfig&) override { return Status(); }
  uint16_t count() const override { return 1; }
  bool getByIndex(uint16_t index, Destination& destination) const override {
    if (index != 0)
      return false;
    destination.id = 3;
    destination.enabled = enabled_;
    destination.label[0] = 'L';
    destination.label[1] = '\0';
    return true;
  }
  bool findById(DeviceId id, Destination& destination) const override {
    return id == 3 && getByIndex(0, destination);
  }
  bool isEnabled(DeviceId id) const override { return id == 3 && enabled_; }
  void setEnabled(bool enabled) { enabled_ = enabled; }

private:
  bool enabled_;
};

int main() {
  FixedRequestRepository<4> repository;
  assert(repository.configure().ok());
  FakeClock clock;
  FakeDestinationCatalog destinations;
  StationRequestLifecycle lifecycle(repository, destinations, clock, 1, 100);
  assert(lifecycle.configure().ok());

  RequestRecord created = {};
  NewRequest input = {};
  input.destinationId = 3;
  assert(lifecycle.create(input, created).ok());
  assert(created.id.originStationId == 1);
  assert(created.id.sequence == 1);
  assert(created.createdAtMs == 100);
  assert(created.expiresAtMs == 200);
  assert(created.state == RequestState::PENDING_UNASSIGNED);

  RequestRecord found = {};
  assert(lifecycle.find(created.id, found));
  assert(found.destinationId == 3);

  destinations.setEnabled(false);
  RequestRecord rejected = {};
  assert(lifecycle.create(input, rejected).code == ErrorCode::INVALID_ARGUMENT);
  destinations.setEnabled(true);

  // Cancellation remains a station-authoritative, idempotent transition.
  RequestRecord cancelledRequest = {};
  assert(lifecycle.create(input, cancelledRequest).ok());
  assert(lifecycle.cancel(cancelledRequest.id, CancellationSource::STUDENT).ok());
  assert(lifecycle.cancel(cancelledRequest.id, CancellationSource::STUDENT).ok());
  assert(lifecycle.find(cancelledRequest.id, found));
  assert(found.state == RequestState::CANCELLED);

  // An assigned ride is not subject to the unassigned-request expiry timer.
  RequestRecord assignedRequest = {};
  assert(lifecycle.create(input, assignedRequest).ok());
  RequestRecord updated = {};
  assert(repository.beginClaimIfEligible(assignedRequest.id, 8, 80, 101, updated).ok());
  assert(repository
             .commitAssignmentIfClaimPending(assignedRequest.id, 8, 80, 1, 102, updated)
             .ok());
  JourneyUpdate journey = {};
  journey.requestId = assignedRequest.id;
  journey.assignedCartId = 8;
  journey.status = JourneyStatus::ON_THE_WAY;
  journey.eventSequence = 1;
  assert(lifecycle.applyJourneyUpdate(journey).ok());
  assert(lifecycle.cancel(assignedRequest.id, CancellationSource::STUDENT).code ==
         ErrorCode::ALREADY_ASSIGNED);

  clock.set(200);
  RequestId expiredIds[2] = {};
  ExpiredRequestSet expired = {0, expiredIds, 2};
  assert(lifecycle.expireDue(clock.millisNow(), expired).ok());
  assert(expired.count == 1);
  assert(RequestIdentity::equals(expiredIds[0], created.id));
  assert(lifecycle.find(created.id, found));
  assert(found.state == RequestState::EXPIRED);
  assert(lifecycle.find(assignedRequest.id, found));
  assert(found.state == RequestState::ON_THE_WAY);
  assert(lifecycle.find(cancelledRequest.id, found));
  assert(found.state == RequestState::CANCELLED);

  // Expiry output capacity is checked before any mutation.
  FixedRequestRepository<2> smallRepository;
  assert(smallRepository.configure().ok());
  StationRequestLifecycle smallLifecycle(smallRepository, destinations, clock, 2, 10);
  assert(smallLifecycle.configure().ok());
  clock.set(300);
  RequestRecord first = {};
  RequestRecord second = {};
  assert(smallLifecycle.create(input, first).ok());
  assert(smallLifecycle.create(input, second).ok());
  clock.set(310);
  RequestId onlyOne[1] = {};
  ExpiredRequestSet insufficient = {0, onlyOne, 1};
  assert(smallLifecycle.expireDue(clock.millisNow(), insufficient).code ==
         ErrorCode::BUFFER_FULL);
  assert(smallLifecycle.find(first.id, found));
  assert(found.state == RequestState::PENDING_UNASSIGNED);

  FixedRequestRepository<2> retentionRepository;
  assert(retentionRepository.configure().ok());
  clock.set(500);
  StationRequestLifecycle retentionLifecycle(retentionRepository, destinations, clock,
                                              3, 100, 50);
  assert(retentionLifecycle.configure().ok());
  RequestRecord retained = {};
  assert(retentionLifecycle.create(input, retained).ok());
  assert(retentionLifecycle.cancel(retained.id, CancellationSource::STUDENT).ok());
  clock.set(60500);
  ExpiredRequestSet noExpiries = {0, 0, 0};
  assert(retentionLifecycle.expireDue(clock.millisNow(), noExpiries).ok());
  assert(!retentionLifecycle.find(retained.id, found));
  return 0;
}
