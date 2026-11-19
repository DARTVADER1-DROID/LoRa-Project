#pragma once

#include "ports.hpp"

namespace ca {

struct EncodeResult {
  Status status;
  uint16_t bytesWritten;
};

struct DecodeResult {
  Status status;
  uint16_t bytesRead;
};

struct ProtocolEvent {
  ProtocolEventType type;
  ProtocolMessage message;
};

class FrameCodec {
 public:
  virtual ~FrameCodec() {}
  virtual EncodeResult encode(const ProtocolMessage& message,
                              MutableFrameView output) const = 0;
  virtual DecodeResult decode(FrameView input, ProtocolMessage& message) const = 0;
};

class MessageValidator {
 public:
  virtual ~MessageValidator() {}
  virtual Status validate(const ProtocolMessage& message,
                          const DeviceConfig& localConfig) const = 0;
};

class ProtocolEndpoint {
 public:
  virtual ~ProtocolEndpoint() {}
  virtual Status begin() = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual Status enqueue(const ProtocolMessage& message,
                         DeliveryClass deliveryClass) = 0;
  virtual bool pollEvent(ProtocolEvent& event) = 0;
};

class RetryManager {
 public:
  virtual ~RetryManager() {}
  virtual Status schedule(const ProtocolMessage& message, uint32_t nowMs) = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual void onAcknowledgement(TransactionId transactionId) = 0;
  virtual void cancel(TransactionId transactionId) = 0;
};

}  // namespace ca
