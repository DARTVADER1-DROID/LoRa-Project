#include "ca/input/debounced_button_input.hpp"

#include <assert.h>

using namespace ca;

class FakeButtonReader : public IButtonPinReader {
public:
  FakeButtonReader() : state_{false, false}, begun_(false) {}
  Status begin(const ButtonConfig& config) override {
    begun_ = config.buttonCount == 2;
    return begun_ ? Status() : Status(ErrorCode::INVALID_ARGUMENT);
  }
  bool isPressed(uint8_t index) const override { return state_[index]; }

  bool state_[2];
  bool begun_;
};

int main() {
  FakeButtonReader reader;
  DebouncedButtonInput input(reader);
  const ButtonConfig config = {2, 20, 600};
  assert(input.begin(config).ok());
  assert(reader.begun_);
  assert(input.begin(config).code == ErrorCode::INVALID_STATE);

  input.service(0);
  reader.state_[0] = true;
  input.service(10);
  input.service(30);
  reader.state_[0] = false;
  input.service(40);
  input.service(60);

  ButtonEvent event = {};
  assert(input.pollEvent(event));
  assert(event.type == ButtonEventType::SHORT_PRESS);
  assert(input.pollEvent(event));
  assert(event.type == ButtonEventType::RELEASE);
  assert(!input.pollEvent(event));
  assert(input.lastStatus().ok());
  return 0;
}
