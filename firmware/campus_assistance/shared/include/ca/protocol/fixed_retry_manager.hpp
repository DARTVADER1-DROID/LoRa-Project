#pragma once

#include "ca/protocol.hpp"

namespace ca {

// Allocation-free exponential retry scheduler. Capacity and timing policy are
// selected by the target composition after airtime and RAM budgeting.
template <uint16_t StorageCapacity>
class FixedRetryManager final : public RetryManager {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");
  static_assert(StorageCapacity <= 32767, "Outcome capacity exceeds uint16_t");

public:
  explicit FixedRetryManager(const RetryPolicy& policy)
      : policy_(policy), nowMs_(0), outcomeHead_(0), outcomeCount_(0) {
    for (uint16_t i = 0; i < StorageCapacity; ++i)
      entries_[i] = Entry();
  }

  Status schedule(const ProtocolMessage& message, uint32_t nowMs) override {
    if (message.header.transactionId == 0 ||
        message.payloadLength > kMaxProtocolPayloadBytes || policy_.maxAttempts == 0 ||
        policy_.initialBackoffMs == 0 ||
        policy_.maxBackoffMs < policy_.initialBackoffMs ||
        policy_.maxBackoffMs > 0x7FFFFFFFUL)
      return Status(ErrorCode::INVALID_ARGUMENT);

    nowMs_ = nowMs;
    for (uint16_t i = 0; i < StorageCapacity; ++i) {
      if (!entries_[i].active ||
          entries_[i].message.header.transactionId != message.header.transactionId)
        continue;
      return sameMessage(entries_[i].message, message)
                 ? Status()
                 : Status(ErrorCode::INVALID_STATE);
    }

    for (uint16_t i = 0; i < StorageCapacity; ++i) {
      if (entries_[i].active)
        continue;
      Entry& entry = entries_[i];
      entry.active = true;
      entry.awaitingAttempt = false;
      entry.terminalPending = false;
      entry.message = message;
      entry.attemptsUsed = 0;
      entry.nextDelayMs = 0;
      entry.dueAtMs = nowMs;
      return Status();
    }
    return Status(ErrorCode::QUEUE_FULL);
  }

  void service(uint32_t nowMs) override {
    nowMs_ = nowMs;
    for (uint16_t i = 0; i < StorageCapacity; ++i) {
      Entry& entry = entries_[i];
      if (entry.active && entry.terminalPending && !outcomesFull()) {
        pushOutcome(entry.message.header.transactionId,
                    entry.terminalOutcome,
                    entry.attemptsUsed);
        entry = Entry();
      }
    }
  }

  bool pollDue(ProtocolMessage& message, uint8_t& attemptNumber) override {
    for (uint16_t i = 0; i < StorageCapacity; ++i) {
      Entry& entry = entries_[i];
      if (!entry.active || entry.awaitingAttempt || entry.terminalPending ||
          !hasElapsed(nowMs_, entry.dueAtMs, entry.nextDelayMs))
        continue;
      entry.awaitingAttempt = true;
      message = entry.message;
      attemptNumber = static_cast<uint8_t>(entry.attemptsUsed + 1U);
      return true;
    }
    return false;
  }

  Status onTransmissionAttempt(TransactionId transactionId,
                               bool accepted,
                               uint32_t nowMs) override {
    (void)accepted; // A rejected transport submission still consumes an attempt.
    Entry* entry = find(transactionId);
    if (entry == 0 || !entry->awaitingAttempt)
      return Status(ErrorCode::NOT_FOUND);

    nowMs_ = nowMs;
    entry->awaitingAttempt = false;
    if (entry->attemptsUsed < 0xFFU)
      ++entry->attemptsUsed;

    if (entry->attemptsUsed >= policy_.maxAttempts) {
      finish(*entry, RetryOutcomeCode::EXHAUSTED);
      return Status();
    }

    entry->dueAtMs = nowMs;
    if (entry->nextDelayMs == 0) {
      entry->nextDelayMs = policy_.initialBackoffMs;
    } else if (entry->nextDelayMs >= policy_.maxBackoffMs / 2U) {
      entry->nextDelayMs = policy_.maxBackoffMs;
    } else {
      entry->nextDelayMs *= 2U;
    }
    if (entry->nextDelayMs > policy_.maxBackoffMs)
      entry->nextDelayMs = policy_.maxBackoffMs;
    return Status();
  }

  Status onAcknowledgement(TransactionId transactionId) override {
    Entry* entry = find(transactionId);
    if (entry == 0)
      return Status(ErrorCode::NOT_FOUND);
    finish(*entry, RetryOutcomeCode::ACKNOWLEDGED);
    return Status();
  }

  Status cancel(TransactionId transactionId) override {
    Entry* entry = find(transactionId);
    if (entry == 0)
      return Status(ErrorCode::NOT_FOUND);
    finish(*entry, RetryOutcomeCode::CANCELLED);
    return Status();
  }

  bool pollOutcome(RetryOutcome& outcome) override {
    if (outcomeCount_ == 0)
      return false;
    outcome = outcomes_[outcomeHead_];
    outcomeHead_ = static_cast<uint16_t>((outcomeHead_ + 1U) % kOutcomeCapacity);
    --outcomeCount_;
    return true;
  }

  uint16_t pendingCount() const {
    uint16_t count = 0;
    for (uint16_t i = 0; i < StorageCapacity; ++i)
      if (entries_[i].active)
        ++count;
    return count;
  }

private:
  static const uint16_t kOutcomeCapacity = static_cast<uint16_t>(StorageCapacity * 2U);

  struct Entry {
    bool active;
    bool awaitingAttempt;
    bool terminalPending;
    ProtocolMessage message;
    uint8_t attemptsUsed;
    uint32_t nextDelayMs;
    uint32_t dueAtMs;
    RetryOutcomeCode terminalOutcome;

    Entry()
        : active(false), awaitingAttempt(false), terminalPending(false), message(),
          attemptsUsed(0), nextDelayMs(0), dueAtMs(0),
          terminalOutcome(RetryOutcomeCode::CANCELLED) {}
  };

  RetryPolicy policy_;
  uint32_t nowMs_;
  Entry entries_[StorageCapacity];
  RetryOutcome outcomes_[kOutcomeCapacity];
  uint16_t outcomeHead_;
  uint16_t outcomeCount_;

  bool outcomesFull() const { return outcomeCount_ >= kOutcomeCapacity; }

  void
  pushOutcome(TransactionId transactionId, RetryOutcomeCode code, uint8_t attempts) {
    const uint16_t tail =
        static_cast<uint16_t>((outcomeHead_ + outcomeCount_) % kOutcomeCapacity);
    outcomes_[tail].transactionId = transactionId;
    outcomes_[tail].outcome = code;
    outcomes_[tail].attemptsUsed = attempts;
    ++outcomeCount_;
  }

  void finish(Entry& entry, RetryOutcomeCode code) {
    if (outcomesFull()) {
      entry.awaitingAttempt = false;
      entry.terminalPending = true;
      entry.terminalOutcome = code;
      return;
    }
    pushOutcome(entry.message.header.transactionId, code, entry.attemptsUsed);
    entry = Entry();
  }

  Entry* find(TransactionId transactionId) {
    for (uint16_t i = 0; i < StorageCapacity; ++i)
      if (entries_[i].active &&
          entries_[i].message.header.transactionId == transactionId)
        return &entries_[i];
    return 0;
  }

  static bool sameMessage(const ProtocolMessage& left, const ProtocolMessage& right) {
    if (left.header.protocolVersion != right.header.protocolVersion ||
        left.header.networkId != right.header.networkId ||
        left.header.messageType != right.header.messageType ||
        left.header.sourceDeviceId != right.header.sourceDeviceId ||
        left.header.destinationDeviceId != right.header.destinationDeviceId ||
        left.header.transactionId != right.header.transactionId ||
        left.header.messageSequence != right.header.messageSequence ||
        left.payloadLength != right.payloadLength)
      return false;
    for (uint16_t i = 0; i < left.payloadLength; ++i)
      if (left.payload[i] != right.payload[i])
        return false;
    return true;
  }
};

} // namespace ca
