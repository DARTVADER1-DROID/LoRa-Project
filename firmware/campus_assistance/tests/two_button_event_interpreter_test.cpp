#include "ca/input/two_button_event_interpreter.hpp"

#include <assert.h>

using namespace ca;

int main() {
  TwoButtonEventInterpreter station(DeviceRole::STATION);
  TwoButtonEventInterpreter cart(DeviceRole::CART);
  const UiContext context = {};

  ButtonEvent event = {0, ButtonEventType::SHORT_PRESS, 10};
  assert(station.interpret(event, context).type == UiIntentType::NEXT);
  assert(cart.interpret(event, context).type == UiIntentType::NONE);

  event.buttonIndex = 1;
  assert(station.interpret(event, context).type == UiIntentType::CONFIRM);
  assert(cart.interpret(event, context).type == UiIntentType::CONFIRM);

  event.buttonIndex = 0;
  event.type = ButtonEventType::LONG_PRESS;
  assert(station.interpret(event, context).type == UiIntentType::CANCEL);
  assert(cart.interpret(event, context).type == UiIntentType::TOGGLE_ACCEPTANCE);

  event.buttonIndex = 1;
  assert(station.interpret(event, context).type == UiIntentType::NONE);
  assert(cart.interpret(event, context).type == UiIntentType::ADVANCE_JOURNEY);

  event.type = ButtonEventType::RELEASE;
  assert(station.interpret(event, context).type == UiIntentType::NONE);
  assert(cart.interpret(event, context).type == UiIntentType::NONE);

  event.buttonIndex = 2;
  event.type = ButtonEventType::SHORT_PRESS;
  assert(station.interpret(event, context).type == UiIntentType::NONE);
  assert(cart.interpret(event, context).type == UiIntentType::NONE);
  return 0;
}
