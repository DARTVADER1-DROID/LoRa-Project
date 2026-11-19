#pragma once

#include "types.hpp"

namespace ca {

class IStationRegistry;
class ICartRegistry;

struct EncodeResult {
  Status status;
  uint16_t bytesWritten;
};

struct DecodeResult {
  Status status;
  uint16_t bytesRead;
};

union ProtocolEventPayload {
  RequestSummary request;
  ClaimRequest claim;
  ClaimWithdraw claimWithdraw;
  ClaimResult claimResult;
  AssignmentNotice assignment;
  CancellationNotice cancellation;
  ExpiryNotice expiry;
  JourneyUpdate journeyUpdate;
  RequestSnapshot snapshot;
  RequestId snapshotQuery;
  TransactionId acknowledgedTransaction;
};

// The validated wire envelope is retained end-to-end for authorization,
// replay detection, deduplication and ACK correlation. NETWORK_MEMBER_VALIDATED
// is not cryptographic authentication.
struct ProtocolEvent {
  ProtocolEventType type;
  ProtocolHeader envelope;
  SenderValidation senderValidation;
  ProtocolEventPayload payload;
};

struct RetryPolicy {
  uint8_t maxAttempts;
  uint32_t initialBackoffMs;
  uint32_t maxBackoffMs;
};

struct RetryOutcome {
  TransactionId transactionId;
  uint32_t messageSequence;
  RetryOutcomeCode outcome;
  uint8_t attemptsUsed;
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
  virtual Status authorizeSender(const ProtocolHeader& envelope,
                                 const DeviceConfig& localConfig,
                                 SenderValidation& validation) const = 0;
};

// Strict typed payload conversion for the current provisional message layouts.
class MessagePayloadCodec {
public:
  static Status decode(const ProtocolMessage& message, ProtocolEvent& event);
  static Status encode(const ProtocolEvent& event, ProtocolMessage& message);
};

// Provisional reference framing for host tests. Numeric layout, magic and CRC
// choice are not frozen until the protocol schema receives a separate review.
class BinaryFrameCodec final : public FrameCodec {
public:
  EncodeResult encode(const ProtocolMessage& message,
                      MutableFrameView output) const override;
  DecodeResult decode(FrameView input, ProtocolMessage& message) const override;
};

// Structural network membership checks only; this is not cryptographic identity.
class DefaultMessageValidator final : public MessageValidator {
public:
  explicit DefaultMessageValidator(const IStationRegistry& stations);
  explicit DefaultMessageValidator(const ICartRegistry& carts);
  DefaultMessageValidator(const IStationRegistry& stations, const ICartRegistry& carts);

  Status validate(const ProtocolMessage& message,
                  const DeviceConfig& localConfig) const override;
  Status authorizeSender(const ProtocolHeader& envelope,
                         const DeviceConfig& localConfig,
                         SenderValidation& validation) const override;

private:
  const IStationRegistry* stations_;
  const ICartRegistry* carts_;
};

class ProtocolEndpoint {
public:
  virtual ~ProtocolEndpoint() {}
  virtual Status begin() = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual Status enqueue(const ProtocolMessage& message,
                         DeliveryClass deliveryClass) = 0;
  virtual bool pollEvent(ProtocolEvent& event) = 0;
  virtual bool pollRetryOutcome(RetryOutcome& outcome) = 0;
};

class RetryManager {
public:
  virtual ~RetryManager() {}
  virtual Status schedule(const ProtocolMessage& message, uint32_t nowMs) = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual bool pollDue(ProtocolMessage& message, uint8_t& attemptNumber) = 0;
  virtual Status onTransmissionAttempt(TransactionId transactionId,
                                       uint32_t messageSequence,
                                       bool accepted,
                                       uint32_t nowMs) = 0;
  virtual Status onAcknowledgement(TransactionId transactionId,
                                   DeviceId acknowledgingDeviceId,
                                   uint32_t acknowledgedMessageSequence) = 0;
  virtual Status cancel(TransactionId transactionId,
                        uint32_t messageSequence) = 0;
  virtual bool pollOutcome(RetryOutcome& outcome) = 0;
  virtual uint16_t capacity() const = 0;
  virtual uint16_t pendingCount() const = 0;
};

} // namespace ca
