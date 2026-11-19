#pragma once

#include "ca/protocol.hpp"
#include "ca/protocol/fixed_retry_manager.hpp"
#include "ca/ports/hardware.hpp"

namespace ca {

// Bounded protocol adapter joining frame codec, network validation, radio
// transport and retry scheduling. It owns only the typed event queue.
template <uint8_t EventCapacity>
class FixedProtocolEndpoint final : public ProtocolEndpoint {
  static_assert(EventCapacity > 0, "EventCapacity must be nonzero");

public:
  FixedProtocolEndpoint(FrameCodec& codec,
                        MessageValidator& validator,
                        IRadioTransport& transport,
                        RetryManager& retries,
                        const DeviceConfig& config,
                        const RadioProfile& radioProfile)
      : codec_(codec), validator_(validator), transport_(transport), retries_(retries),
        config_(config), radioProfile_(radioProfile), eventHead_(0), eventCount_(0),
        nowMs_(0), initialized_(false) {}

  Status begin() override {
    if (initialized_ || config_.deviceId == 0 || config_.networkId == 0 ||
        config_.protocolVersion == 0)
      return Status(ErrorCode::CONFIGURATION_INVALID);
    const Status status = transport_.begin(radioProfile_);
    if (!status.ok())
      return status;
    initialized_ = true;
    return Status();
  }

  void service(uint32_t nowMs) override {
    if (!initialized_)
      return;
    nowMs_ = nowMs;
    transport_.service();
    retries_.service(nowMs_);

    for (uint8_t sent = 0; sent < EventCapacity; ++sent) {
      ProtocolMessage message = {};
      uint8_t attemptNumber = 0;
      if (!retries_.pollDue(message, attemptNumber))
        break;
      (void)attemptNumber;
      const Status status = sendMessage(message);
      retries_.onTransmissionAttempt(message.header.transactionId, status.ok(), nowMs_);
    }

    for (uint8_t received = 0; received < EventCapacity; ++received) {
      if (eventCount_ >= EventCapacity)
        break;
      ReceivedFrame frame = {};
      if (!transport_.pollReceived(frame))
        break;
      processFrame(frame);
    }
  }

  Status enqueue(const ProtocolMessage& message, DeliveryClass deliveryClass) override {
    if (!initialized_)
      return Status(ErrorCode::INVALID_STATE);
    if (message.header.sourceDeviceId != config_.deviceId ||
        message.header.networkId != config_.networkId ||
        message.header.protocolVersion != config_.protocolVersion ||
        message.payloadLength > kMaxProtocolPayloadBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);

    ProtocolEvent validationEvent = {};
    const Status payloadStatus = MessagePayloadCodec::decode(message, validationEvent);
    if (!payloadStatus.ok())
      return payloadStatus;

    if (deliveryClass == DeliveryClass::RETRY_UNTIL_ACK)
      return retries_.schedule(message, nowMs_);
    if (deliveryClass != DeliveryClass::BEST_EFFORT &&
        deliveryClass != DeliveryClass::PRIORITY)
      return Status(ErrorCode::INVALID_ARGUMENT);
    return sendMessage(message);
  }

  bool pollEvent(ProtocolEvent& event) override {
    if (eventCount_ == 0)
      return false;
    event = events_[eventHead_];
    eventHead_ = static_cast<uint8_t>((eventHead_ + 1U) % EventCapacity);
    --eventCount_;
    return true;
  }

  bool pollRetryOutcome(RetryOutcome& outcome) { return retries_.pollOutcome(outcome); }

  uint8_t queuedEventCount() const { return eventCount_; }

private:
  FrameCodec& codec_;
  MessageValidator& validator_;
  IRadioTransport& transport_;
  RetryManager& retries_;
  DeviceConfig config_;
  RadioProfile radioProfile_;
  ProtocolEvent events_[EventCapacity];
  uint8_t eventHead_;
  uint8_t eventCount_;
  uint32_t nowMs_;
  bool initialized_;

  Status sendMessage(const ProtocolMessage& message) {
    uint8_t bytes[kMaxFrameBytes] = {};
    MutableFrameView output = {bytes, kMaxFrameBytes};
    const EncodeResult encoded = codec_.encode(message, output);
    if (!encoded.status.ok())
      return encoded.status;
    const FrameView frame = {bytes, encoded.bytesWritten};
    return transport_.send(frame);
  }

  void processFrame(const ReceivedFrame& frame) {
    if (frame.length == 0 || frame.length > kMaxFrameBytes)
      return;

    ProtocolMessage message = {};
    const FrameView input = {frame.bytes, frame.length};
    const DecodeResult decoded = codec_.decode(input, message);
    if (!decoded.status.ok() || !validator_.validate(message, config_).ok())
      return;

    SenderValidation senderValidation = SenderValidation::UNVALIDATED;
    if (!validator_.authorizeSender(message.header, config_, senderValidation).ok() ||
        senderValidation == SenderValidation::UNVALIDATED)
      return;

    if (message.header.messageType == MessageType::ACKNOWLEDGEMENT) {
      retries_.onAcknowledgement(message.header.transactionId);
      return;
    }

    ProtocolEvent event = {};
    if (!MessagePayloadCodec::decode(message, event).ok())
      return;
    event.senderValidation = senderValidation;

    const uint8_t tail =
        static_cast<uint8_t>((eventHead_ + eventCount_) % EventCapacity);
    events_[tail] = event;
    ++eventCount_;

    if (message.header.transactionId != 0)
      sendAcknowledgement(message.header);
  }

  void sendAcknowledgement(const ProtocolHeader& received) {
    ProtocolMessage acknowledgement = {};
    acknowledgement.header.protocolVersion = config_.protocolVersion;
    acknowledgement.header.networkId = config_.networkId;
    acknowledgement.header.messageType = MessageType::ACKNOWLEDGEMENT;
    acknowledgement.header.sourceDeviceId = config_.deviceId;
    acknowledgement.header.destinationDeviceId = received.sourceDeviceId;
    acknowledgement.header.transactionId = received.transactionId;
    acknowledgement.header.messageSequence = received.messageSequence;
    acknowledgement.payloadLength = 0;
    sendMessage(acknowledgement);
  }
};

} // namespace ca
