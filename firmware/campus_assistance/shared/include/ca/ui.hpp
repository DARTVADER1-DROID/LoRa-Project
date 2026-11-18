#pragma once
#include "types.hpp"
namespace ca {
struct UiContext;
struct CartUiContext;
struct UiIntent;
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
