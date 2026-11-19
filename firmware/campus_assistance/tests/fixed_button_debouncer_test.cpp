#include "ca/input/fixed_button_debouncer.hpp"

#include <assert.h>
#include <stdint.h>

using namespace ca;

int main() {
  FixedButtonDebouncer buttons;
  const ButtonConfig config = {2, 20, 700};
  assert(buttons.configure(config).ok());
  assert(buttons.configure(config).code == ErrorCode::INVALID_STATE);

  bool state[2] = {false, false};
  assert(buttons.update(state, 2, 0).ok());

  state[0] = true;
  assert(buttons.update(state, 2, 10).ok());
  assert(buttons.update(state, 2, 30).ok());
  assert(buttons.update(state, 2, 729).ok());
  ButtonEvent event = {};
  assert(!buttons.pollEvent(event));
  assert(buttons.update(state, 2, 730).ok());
  assert(buttons.pollEvent(event));
  assert(event.buttonIndex == 0);
  assert(event.type == ButtonEventType::LONG_PRESS);

  state[0] = false;
  assert(buttons.update(state, 2, 800).ok());
  assert(buttons.update(state, 2, 820).ok());
  assert(buttons.pollEvent(event));
  assert(event.type == ButtonEventType::RELEASE);
  assert(!buttons.pollEvent(event));

  state[1] = true;
  assert(buttons.update(state, 2, 1000).ok());
  assert(buttons.update(state, 2, 1020).ok());
  state[1] = false;
  assert(buttons.update(state, 2, 1100).ok());
  assert(buttons.update(state, 2, 1120).ok());
  assert(buttons.pollEvent(event));
  assert(event.buttonIndex == 1);
  assert(event.type == ButtonEventType::SHORT_PRESS);
  assert(buttons.pollEvent(event));
  assert(event.type == ButtonEventType::RELEASE);

  // A press shorter than the debounce interval is ignored.
  state[0] = true;
  assert(buttons.update(state, 2, 1200).ok());
  state[0] = false;
  assert(buttons.update(state, 2, 1205).ok());
  assert(buttons.update(state, 2, 1230).ok());
  assert(!buttons.pollEvent(event));

  // Debounce and long-press timing remain correct across millis() rollover.
  const uint32_t nearWrap = UINT32_MAX - 10U;
  state[0] = true;
  assert(buttons.update(state, 2, nearWrap).ok());
  assert(buttons.update(state, 2, 9U).ok());
  assert(buttons.update(state, 2, 709U).ok());
  assert(buttons.pollEvent(event));
  assert(event.type == ButtonEventType::LONG_PRESS);
  return 0;
}
