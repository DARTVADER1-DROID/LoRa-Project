#pragma once
#include "types.hpp"
namespace ca {
struct UiContext { uint8_t screenId; bool requestActive; };
struct CartUiContext { uint8_t screenId; DriverMode driverMode; bool activeAssignment; };
enum class UiIntentType:uint8_t { NONE, NEXT, PREVIOUS, CONFIRM, CANCEL, ENABLE_ACCEPTANCE, DISABLE_ACCEPTANCE, ADVANCE_JOURNEY };
struct UiIntent { UiIntentType type; uint8_t argument; };
class ButtonEventInterpreter {
public:
  UiIntent interpret(const ButtonEvent& event, const UiContext& context) const;
};
class StationUserInterface {
public:
  void onIntent(const UiIntent& intent);
  UiViewModel view() const;
};
class CartUserInterface {
public:
  void onIntent(const UiIntent& intent, const CartUiContext& context);
  UiViewModel view() const;
};
}
