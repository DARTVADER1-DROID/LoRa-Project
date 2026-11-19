#pragma once

#include "ca/types.hpp"

namespace ca {

class IRadioTransport {
public:
  virtual ~IRadioTransport() {}
  virtual Status begin(const RadioProfile& profile) = 0;
  virtual RadioHealth health() const = 0;
  virtual Status send(FrameView frame) = 0;
  virtual bool pollReceived(ReceivedFrame& frame) = 0;
  virtual void service() = 0;
};

class IButtonInput {
public:
  virtual ~IButtonInput() {}
  virtual Status begin(const ButtonConfig& config) = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual bool pollEvent(ButtonEvent& event) = 0;
};

class IDisplay {
public:
  virtual ~IDisplay() {}
  virtual Status begin() = 0;
  virtual void render(const UiViewModel& view) = 0;
};

class IBuzzer {
public:
  virtual ~IBuzzer() {}
  virtual Status begin(const BuzzerConfig& config) = 0;
  virtual void playCue(uint8_t cueId) = 0;
  virtual void service(uint32_t nowMs) = 0;
};

} // namespace ca
