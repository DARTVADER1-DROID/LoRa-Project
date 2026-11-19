#include "CampusAssistance.h"

#include <assert.h>

using namespace ca;

namespace {

void testValidity() {
  RequestId id = {1, 0, 0};
  assert(RequestIdentity::isValid(id));
  id.originStationId = 0;
  assert(!RequestIdentity::isValid(id));
}

void testEqualityUsesFullIdentity() {
  const RequestId baseline = {7, 42, 3};
  assert(RequestIdentity::equals(baseline, baseline));

  RequestId candidate = baseline;
  ++candidate.originStationId;
  assert(!RequestIdentity::equals(baseline, candidate));

  candidate = baseline;
  ++candidate.sequence;
  assert(!RequestIdentity::equals(baseline, candidate));

  candidate = baseline;
  ++candidate.generation;
  assert(!RequestIdentity::equals(baseline, candidate));

  candidate = baseline;
  candidate.sequence = 0;
  candidate.generation = 0;
  assert(!RequestIdentity::equals(baseline, candidate));
}

void testHashConsistencyAndFieldCoverage() {
  const RequestId baseline = {7, 42, 3};
  const uint32_t expected = RequestIdentity::hash(baseline);
  assert(expected == RequestIdentity::hash(baseline));

  RequestId candidate = baseline;
  ++candidate.originStationId;
  assert(expected != RequestIdentity::hash(candidate));

  candidate = baseline;
  ++candidate.sequence;
  assert(expected != RequestIdentity::hash(candidate));

  candidate = baseline;
  ++candidate.generation;
  assert(expected != RequestIdentity::hash(candidate));
}

} // namespace

int main() {
  testValidity();
  testEqualityUsesFullIdentity();
  testHashConsistencyAndFieldCoverage();
  return 0;
}
