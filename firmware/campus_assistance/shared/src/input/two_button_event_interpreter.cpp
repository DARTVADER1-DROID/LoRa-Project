#include "ca/input/two_button_event_interpreter.hpp"

namespace ca {

UiIntent TwoButtonEventInterpreter::interpret(const ButtonEvent& event,
                                              const UiContext& context) const {
  (void)context;
  UiIntent intent = {UiIntentType::NONE, 0};
  if (event.buttonIndex >= 2 || event.type == ButtonEventType::RELEASE)
    return intent;

  if (role_ == DeviceRole::STATION) {
    if (event.buttonIndex == 0 && event.type == ButtonEventType::SHORT_PRESS)
      intent.type = UiIntentType::NEXT;
    else if (event.buttonIndex == 1 && event.type == ButtonEventType::SHORT_PRESS)
      intent.type = UiIntentType::CONFIRM;
    else if (event.buttonIndex == 0 && event.type == ButtonEventType::LONG_PRESS)
      intent.type = UiIntentType::CANCEL;
    return intent;
  }

  if (role_ == DeviceRole::CART) {
    if (event.buttonIndex == 0 && event.type == ButtonEventType::LONG_PRESS)
      intent.type = UiIntentType::TOGGLE_ACCEPTANCE;
    else if (event.buttonIndex == 1 && event.type == ButtonEventType::SHORT_PRESS)
      intent.type = UiIntentType::CONFIRM;
    else if (event.buttonIndex == 1 && event.type == ButtonEventType::LONG_PRESS)
      intent.type = UiIntentType::ADVANCE_JOURNEY;
  }
  return intent;
}

} // namespace ca
