#include "CampusAssistance.h"
#include "ca/storage/fixed_destination_catalog.hpp"

#include <assert.h>
#include <string.h>

using namespace ca;

int main() {
  FixedDestinationCatalog<3> catalog;
  Destination entries[2] = {};
  entries[0].id = 1;
  strcpy(entries[0].label, "Library");
  entries[0].enabled = true;
  entries[1].id = 2;
  strcpy(entries[1].label, "Canteen");
  entries[1].enabled = false;

  DestinationConfig config = {2, entries, 2};
  assert(catalog.load(config).ok());
  assert(catalog.count() == 2);
  assert(catalog.isEnabled(1));
  assert(!catalog.isEnabled(2));

  Destination found = {};
  assert(catalog.findById(2, found));
  assert(strcmp(found.label, "Canteen") == 0);
  assert(!catalog.getByIndex(2, found));

  entries[1].id = 1;
  assert(catalog.load(config).code == ErrorCode::DUPLICATE);
  assert(catalog.count() == 2);
  assert(catalog.findById(2, found));

  DestinationConfig oversized = {4, entries, 4};
  assert(catalog.load(oversized).code == ErrorCode::INVALID_ARGUMENT);
  return 0;
}
