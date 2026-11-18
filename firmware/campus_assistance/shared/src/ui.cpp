#include "../include/ca/ui.hpp"
namespace ca {
UiIntent ButtonEventInterpreter::interpret(const ButtonEvent& event,const UiContext& context) const {
  // TODO: map debounced semantic button events to context-valid intents; enforce two-button interaction contract.
  (void)event; (void)context;
  UiIntent intent={UiIntentType::NONE,0};
  return intent;
}
void StationUserInterface::onIntent(const UiIntent& intent) {
  // TODO: update station screen/navigation state only; domain validates all state-changing actions.
  (void)intent;
}
UiViewModel StationUserInterface::view() const {
  // TODO: return a bounded 16x2 view model; never claim unconfirmed remote state.
  UiViewModel view={{0},{0},0};
  return view;
}
void CartUserInterface::onIntent(const UiIntent& intent,const CartUiContext& context) {
  // TODO: emit driver intent for application validation; do not mutate assignment ownership here.
  (void)intent; (void)context;
}
UiViewModel CartUserInterface::view() const {
  // TODO: show driver mode, active journey and next valid action on the 16x2 display.
  UiViewModel view={{0},{0},0};
  return view;
}
}
