#pragma once

#include "ca/protocol.hpp"
#include "ca/protocol/fixed_retry_manager.hpp"
#include "ca/ports/hardware.hpp"
#include "ca/ports/network.hpp"

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
        config_(config), radioProfile_(radioProfile), cartRegistry_(0), eventHead_(0),
        eventCount_(0), broadcastHead_(0), broadcastCount_(0), recentHead_(0),
        recentCount_(0), nowMs_(0), initialized_(false) {}

  FixedProtocolEndpoint(FrameCodec& codec,
                        MessageValidator& validator,
                        IRadioTransport& transport,
                        RetryManager& retries,
                        const DeviceConfig& config,
                        const RadioProfile& radioProfile,
                        ICartRegistry& cartRegistry)
      : codec_(codec), validator_(validator), transport_(transport), retries_(retries),
        config_(config), radioProfile_(radioProfile), cartRegistry_(&cartRegistry),
        eventHead_(0), eventCount_(0), broadcastHead_(0), broadcastCount_(0),
        recentHead_(0), recentCount_(0), nowMs_(0), initialized_(false) {}

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
    scheduleBroadcasts();

    const RadioHealth radioHealth = transport_.health();
    if (radioHealth.receiving || radioHealth.faulted) {
      ProtocolMessage message = {};
      uint8_t attemptNumber = 0;
      if (retries_.pollDue(message, attemptNumber)) {
        (void)attemptNumber;
        const Status status = sendMessage(message);
        retries_.onTransmissionAttempt(
            message.header.transactionId, status.ok(), nowMs_);
      }
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
        message.header.transactionId == 0 ||
        message.header.networkId != config_.networkId ||
        message.header.protocolVersion != config_.protocolVersion ||
        message.payloadLength > kMaxProtocolPayloadBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);

    ProtocolEvent validationEvent = {};
    const Status payloadStatus = MessagePayloadCodec::decode(message, validationEvent);
    if (!payloadStatus.ok())
      return payloadStatus;

    if (deliveryClass == DeliveryClass::RETRY_UNTIL_ACK) {
      if (message.header.transactionId == 0)
        return Status(ErrorCode::INVALID_ARGUMENT);
      if (message.header.destinationDeviceId == 0)
        return queueReliableBroadcast(message);
      return retries_.schedule(message, nowMs_);
    }
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

  bool pollRetryOutcome(RetryOutcome& outcome) override {
    return retries_.pollOutcome(outcome);
  }

  uint8_t queuedEventCount() const { return eventCount_; }

private:
  FrameCodec& codec_;
  MessageValidator& validator_;
  IRadioTransport& transport_;
  RetryManager& retries_;
  DeviceConfig config_;
  RadioProfile radioProfile_;
  ICartRegistry* cartRegistry_;
  ProtocolEvent events_[EventCapacity];
  uint8_t eventHead_;
  uint8_t eventCount_;
  struct BroadcastJob {
    ProtocolMessage message;
    uint16_t recipientCount;
    uint16_t nextRecipient;
  };
  BroadcastJob broadcasts_[EventCapacity];
  uint8_t broadcastHead_;
  uint8_t broadcastCount_;
  struct RecentMessage {
    DeviceId sourceDeviceId;
    TransactionId transactionId;
    uint32_t messageSequence;
    MessageType messageType;
    uint32_t frameChecksum;
  };
  static const uint16_t kRecentCapacity = static_cast<uint16_t>(EventCapacity * 2U);
  RecentMessage recent_[kRecentCapacity];
  uint16_t recentHead_;
  uint16_t recentCount_;
  uint32_t nowMs_;
  bool initialized_;

  Status queueReliableBroadcast(const ProtocolMessage& message) {
    if (cartRegistry_ == 0 || cartRegistry_->enabledCount() == 0)
      return Status(ErrorCode::CONFIGURATION_INVALID);
    if (broadcastCount_ >= EventCapacity)
      return Status(ErrorCode::QUEUE_FULL);

    const uint16_t count = cartRegistry_->enabledCount();
    for (uint16_t i = 0; i < count; ++i) {
      const DeviceId cartId = cartRegistry_->enabledCartAt(i);
      if (cartId == 0 || cartId == config_.deviceId)
        return Status(ErrorCode::CONFIGURATION_INVALID);
      for (uint16_t j = 0; j < i; ++j)
        if (cartRegistry_->enabledCartAt(j) == cartId)
          return Status(ErrorCode::DUPLICATE);
    }

    const uint8_t tail = static_cast<uint8_t>(
        (static_cast<uint16_t>(broadcastHead_) + broadcastCount_) % EventCapacity);
    broadcasts_[tail].message = message;
    broadcasts_[tail].recipientCount = count;
    broadcasts_[tail].nextRecipient = 0;
    ++broadcastCount_;
    return Status();
  }

  void scheduleBroadcasts() {
    while (broadcastCount_ > 0 && retries_.pendingCount() < retries_.capacity()) {
      BroadcastJob& job = broadcasts_[broadcastHead_];
      if (job.nextRecipient >= job.recipientCount) {
        broadcastHead_ = static_cast<uint8_t>((broadcastHead_ + 1U) % EventCapacity);
        --broadcastCount_;
        continue;
      }

      const DeviceId cartId = cartRegistry_->enabledCartAt(job.nextRecipient);
      if (cartId == 0 || cartId == config_.deviceId)
        return;

      ProtocolMessage unicast = job.message;
      unicast.header.destinationDeviceId = cartId;
      unicast.header.transactionId =
          job.message.header.transactionId + static_cast<uint32_t>(cartId) * 65537UL;
      if (unicast.header.transactionId == 0)
        unicast.header.transactionId = job.message.header.transactionId;

      const Status scheduled = retries_.schedule(unicast, nowMs_);
      if (!scheduled.ok())
        return;
      ++job.nextRecipient;
      if (job.nextRecipient >= job.recipientCount) {
        broadcastHead_ = static_cast<uint8_t>((broadcastHead_ + 1U) % EventCapacity);
        --broadcastCount_;
      }
    }
  }

  Status sendMessage(const ProtocolMessage& message) {
    uint8_t bytes[kMaxFrameBytes] = {};
    MutableFrameView output = {bytes, kMaxFrameBytes};
    const EncodeResult encoded = codec_.encode(message, output);
    if (!encoded.status.ok())
      return encoded.status;
    const FrameView frame = {bytes, encoded.bytesWritten};
    return transport_.send(frame);
  }

  bool isDuplicate(const ProtocolHeader& header, uint32_t frameChecksum) const {
    for (uint16_t i = 0; i < recentCount_; ++i) {
      const uint16_t index = static_cast<uint16_t>(
          (static_cast<uint32_t>(recentHead_) + i) % kRecentCapacity);
      const RecentMessage& recent = recent_[index];
      if (recent.sourceDeviceId != header.sourceDeviceId ||
          recent.transactionId != header.transactionId ||
          recent.messageSequence != header.messageSequence ||
          recent.messageType != header.messageType)
        continue;
      return recent.frameChecksum == frameChecksum;
    }
    return false;
  }

  bool conflictsWithRecent(const ProtocolHeader& header, uint32_t frameChecksum) const {
    for (uint16_t i = 0; i < recentCount_; ++i) {
      const uint16_t index = static_cast<uint16_t>(
          (static_cast<uint32_t>(recentHead_) + i) % kRecentCapacity);
      const RecentMessage& recent = recent_[index];
      if (recent.sourceDeviceId == header.sourceDeviceId &&
          recent.transactionId == header.transactionId &&
          recent.messageSequence == header.messageSequence &&
          recent.messageType == header.messageType)
        return recent.frameChecksum != frameChecksum;
    }
    return false;
  }

  void remember(const ProtocolHeader& header, uint32_t frameChecksum) {
    uint16_t tail = static_cast<uint16_t>(
        (static_cast<uint32_t>(recentHead_) + recentCount_) % kRecentCapacity);
    if (recentCount_ == kRecentCapacity) {
      recentHead_ = static_cast<uint16_t>((static_cast<uint32_t>(recentHead_) + 1U) %
                                          kRecentCapacity);
      tail = static_cast<uint16_t>(
          (static_cast<uint32_t>(recentHead_) + recentCount_ - 1U) % kRecentCapacity);
    } else {
      ++recentCount_;
    }
    recent_[tail].sourceDeviceId = header.sourceDeviceId;
    recent_[tail].transactionId = header.transactionId;
    recent_[tail].messageSequence = header.messageSequence;
    recent_[tail].messageType = header.messageType;
    recent_[tail].frameChecksum = frameChecksum;
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

    if (message.header.transactionId == 0)
      return;
    if (message.header.messageType == MessageType::ACKNOWLEDGEMENT) {
      retries_.onAcknowledgement(message.header.transactionId,
                                 message.header.sourceDeviceId);
      return;
    }

    ProtocolEvent event = {};
    if (!MessagePayloadCodec::decode(message, event).ok())
      return;
    event.senderValidation = senderValidation;

    const uint16_t checksumOffset = static_cast<uint16_t>(frame.length - 4U);
    const uint32_t frameChecksum =
        (static_cast<uint32_t>(frame.bytes[checksumOffset]) << 24) |
        (static_cast<uint32_t>(frame.bytes[checksumOffset + 1U]) << 16) |
        (static_cast<uint32_t>(frame.bytes[checksumOffset + 2U]) << 8) |
        static_cast<uint32_t>(frame.bytes[checksumOffset + 3U]);

    if (message.header.transactionId != 0) {
      if (conflictsWithRecent(message.header, frameChecksum))
        return;
      if (isDuplicate(message.header, frameChecksum)) {
        sendAcknowledgement(message.header);
        return;
      }
    }

    const uint8_t tail =
        static_cast<uint8_t>((eventHead_ + eventCount_) % EventCapacity);
    events_[tail] = event;
    ++eventCount_;

    if (message.header.transactionId != 0) {
      remember(message.header, frameChecksum);
      sendAcknowledgement(message.header);
    }
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
