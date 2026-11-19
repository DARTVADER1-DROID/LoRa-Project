#pragma once

#include "ca/domain/pending_request_store.hpp"
#include "ca/domain/request_identity.hpp"

namespace ca {

// Fixed-capacity FIFO. StorageCapacity is selected by the target composition
// from measured RAM budgets; configure() may expose a smaller runtime capacity.
template <uint16_t StorageCapacity>
class FixedPendingRequestStore final : public PendingRequestStore {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedPendingRequestStore() : count_(0), capacity_(0) {}

  Status configure(uint16_t capacity) override {
    if (capacity == 0 || capacity > StorageCapacity)
      return Status(ErrorCode::INVALID_ARGUMENT);

    count_ = 0;
    capacity_ = capacity;
    return Status();
  }

  Status insertIfAbsent(const RequestSummary& request) override {
    if (capacity_ == 0 || !RequestIdentity::isValid(request.id) ||
        request.destinationId == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);

    if (contains(request.id))
      return Status(ErrorCode::DUPLICATE);

    if (count_ >= capacity_)
      return Status(ErrorCode::QUEUE_FULL);

    entries_[count_++] = request;
    return Status();
  }

  bool peekOldest(RequestSummary& request) const override {
    if (count_ == 0)
      return false;

    request = entries_[0];
    return true;
  }

  bool remove(const RequestId& id) override { return removeAtFirstMatch(id); }

  uint16_t removeExpired(const ExpiredRequestSet& expired) override {
    if ((expired.count > 0 && expired.items == 0) || expired.count > expired.capacity)
      return 0;

    uint16_t removed = 0;
    uint16_t writeIndex = 0;

    for (uint16_t readIndex = 0; readIndex < count_; ++readIndex) {
      if (containsInSet(entries_[readIndex].id, expired))
        ++removed;
      else
        entries_[writeIndex++] = entries_[readIndex];
    }

    count_ = writeIndex;
    return removed;
  }

  uint16_t removeAssigned(const RequestId& id) override {
    return removeAtFirstMatch(id) ? 1 : 0;
  }

  void clearLocal() override { count_ = 0; }

  bool contains(const RequestId& id) const override {
    for (uint16_t i = 0; i < count_; ++i) {
      if (RequestIdentity::equals(entries_[i].id, id))
        return true;
    }
    return false;
  }

  uint16_t size() const override { return count_; }

  uint16_t capacity() const override { return capacity_; }

private:
  RequestSummary entries_[StorageCapacity];
  uint16_t count_;
  uint16_t capacity_;

  bool removeAtFirstMatch(const RequestId& id) {
    for (uint16_t i = 0; i < count_; ++i) {
      if (!RequestIdentity::equals(entries_[i].id, id))
        continue;

      for (uint16_t j = i; j + 1 < count_; ++j)
        entries_[j] = entries_[j + 1];

      --count_;
      return true;
    }

    return false;
  }

  static bool containsInSet(const RequestId& id, const ExpiredRequestSet& expired) {
    for (uint16_t i = 0; i < expired.count; ++i) {
      if (RequestIdentity::equals(id, expired.items[i]))
        return true;
    }
    return false;
  }
};

} // namespace ca
