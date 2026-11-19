#include "ca/ui/basic_user_interfaces.hpp"

#include <assert.h>
#include <string.h>

using namespace ca;

int main() {
  BasicCartUserInterface ui;
  CartUiContext context = {};
  context.acceptanceMode = AcceptanceMode::NOT_ACCEPTING;
  context.serviceState = CartServiceState::IDLE;
  ui.update(context);
  UiViewModel view = ui.view();
  assert(strncmp(view.line1, "Accept: OFF", 11) == 0);
  assert(strncmp(view.line2, "Ready / Claim", 13) == 0);

  context.acceptanceMode = AcceptanceMode::ACCEPTING;
  context.serviceState = CartServiceState::CLAIM_PENDING;
  ui.update(context);
  view = ui.view();
  assert(strncmp(view.line1, "Accept: ON", 10) == 0);
  assert(strncmp(view.line2, "Claim pending", 13) == 0);

  context.serviceState = CartServiceState::SERVING;
  context.activeAssignment = true;
  ui.update(context);
  view = ui.view();
  assert(strncmp(view.line2, "Serving / Hold B", 16) == 0);

  context.serviceState = CartServiceState::UNCERTAIN;
  ui.update(context);
  view = ui.view();
  assert(strncmp(view.line2, "Check station", 13) == 0);
  return 0;
}
