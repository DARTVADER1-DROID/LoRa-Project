#pragma once

#include "types.hpp"

namespace ca {

struct UiContext {
  uint8_t screenId;
  bool requestActive;
};

struct CartUiContext {
  uint8_t screenId;
  AcceptanceMode acceptanceMode;
  CartServiceState serviceState;
  bool activeAssignment;
};

enum class UiIntentType : uint8_t {
  NONE,
  NEXT,
  PREVIOUS,
  CONFIRM,
  CANCEL,
  ENABLE_ACCEPTANCE,
  DISABLE_ACCEPTANCE,
  ADVANCE_JOURNEY,
  CONFIRM_COMPLETION
};

struct UiIntent {
  UiIntentType type;
  uint8_t argument;
};

class ButtonEventInterpreter {
public:
  virtual ~ButtonEventInterpreter() {}
  virtual UiIntent interpret(const ButtonEvent& event,
                             const UiContext& context) const = 0;
};

class StationUserInterface {
public:
  virtual ~StationUserInterface() {}
  virtual void onIntent(const UiIntent& intent) = 0;
  virtual UiViewModel view() const = 0;
};

class CartUserInterface {
public:
  virtual ~CartUserInterface() {}
  virtual void onIntent(const UiIntent& intent, const CartUiContext& context) = 0;
  virtual UiViewModel view() const = 0;
};

} // namespace ca
