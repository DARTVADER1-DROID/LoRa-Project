#include "CampusAssistance.h"
#include "ca/storage/fixed_pending_request_store.hpp"

#include <assert.h>

using namespace ca;

namespace {

RequestSummary makeSummary(uint32_t sequence, uint16_t generation = 1) {
  RequestSummary summary = {};
  summary.id.originStationId = 10;
  summary.id.sequence = sequence;
  summary.id.generation = generation;
  summary.destinationId = 20;
  summary.localAgeMs = sequence;
  return summary;
}

void testConfigurationAndCapacity() {
  FixedPendingRequestStore<4> store;
  assert(store.capacity() == 0);
  assert(store.configure(0).code == ErrorCode::INVALID_ARGUMENT);
  assert(store.configure(5).code == ErrorCode::INVALID_ARGUMENT);
  assert(store.configure(3).ok());
  assert(store.capacity() == 3);
  assert(store.size() == 0);

  assert(store.insertIfAbsent(makeSummary(1)).ok());
  assert(store.insertIfAbsent(makeSummary(2)).ok());
  assert(store.insertIfAbsent(makeSummary(3)).ok());
  assert(store.insertIfAbsent(makeSummary(4)).code == ErrorCode::QUEUE_FULL);
}

void testDeduplicationUsesCompleteIdentityAndPreservesFifo() {
  FixedPendingRequestStore<4> store;
  RequestSummary oldest = {};
  assert(store.configure(4).ok());
  assert(store.insertIfAbsent(makeSummary(7, 1)).ok());
  assert(store.insertIfAbsent(makeSummary(7, 1)).code == ErrorCode::DUPLICATE);
  assert(store.insertIfAbsent(makeSummary(7, 2)).ok());
  assert(store.insertIfAbsent(makeSummary(8, 1)).ok());

  assert(store.peekOldest(oldest));
  assert(oldest.id.sequence == 7);
  assert(oldest.id.generation == 1);
  assert(store.remove(makeSummary(7, 1).id));
  assert(store.peekOldest(oldest));
  assert(oldest.id.sequence == 7);
  assert(oldest.id.generation == 2);
  assert(store.size() == 2);
}

void testExpiryRemovalAndStableOrder() {
  FixedPendingRequestStore<5> store;
  RequestSummary oldest = {};
  RequestId expiredIds[2] = {makeSummary(2).id, makeSummary(4).id};
  ExpiredRequestSet expired = {2, expiredIds, 2};

  assert(store.configure(5).ok());
  assert(store.insertIfAbsent(makeSummary(1)).ok());
  assert(store.insertIfAbsent(makeSummary(2)).ok());
  assert(store.insertIfAbsent(makeSummary(3)).ok());
  assert(store.insertIfAbsent(makeSummary(4)).ok());
  assert(store.removeExpired(expired) == 2);
  assert(store.size() == 2);
  assert(store.peekOldest(oldest));
  assert(oldest.id.sequence == 1);
  assert(store.removeAssigned(makeSummary(3).id) == 1);
  assert(store.removeAssigned(makeSummary(3).id) == 0);
  assert(store.peekOldest(oldest));
  assert(oldest.id.sequence == 1);
}

void testClearIsLocalAndQueueCanRefill() {
  FixedPendingRequestStore<2> store;
  RequestSummary oldest = {};
  assert(store.configure(2).ok());
  assert(store.insertIfAbsent(makeSummary(11)).ok());
  store.clearLocal();
  assert(store.size() == 0);
  assert(!store.contains(makeSummary(11).id));
  assert(store.insertIfAbsent(makeSummary(12)).ok());
  assert(store.peekOldest(oldest));
  assert(oldest.id.sequence == 12);
}

void testInvalidRequestAndInvalidExpiryView() {
  FixedPendingRequestStore<2> store;
  RequestSummary invalid = makeSummary(1);
  RequestId expiredId = makeSummary(1).id;
  ExpiredRequestSet invalidSet = {1, 0, 1};

  assert(store.configure(2).ok());
  invalid.id.originStationId = 0;
  assert(store.insertIfAbsent(invalid).code == ErrorCode::INVALID_ARGUMENT);
  assert(store.removeExpired(invalidSet) == 0);
  assert(store.size() == 0);
  assert(!store.remove(expiredId));
}

} // namespace

int main() {
  testConfigurationAndCapacity();
  testDeduplicationUsesCompleteIdentityAndPreservesFifo();
  testExpiryRemovalAndStableOrder();
  testClearIsLocalAndQueueCanRefill();
  testInvalidRequestAndInvalidExpiryView();
  return 0;
}
