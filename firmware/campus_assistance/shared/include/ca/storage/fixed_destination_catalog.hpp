#pragma once

#include "ca/domain/destination_catalog.hpp"

namespace ca {

// Fixed-capacity immutable-after-load destination catalogue.
template <uint16_t StorageCapacity>
class FixedDestinationCatalog final : public DestinationCatalog {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedDestinationCatalog() : count_(0), loaded_(false) {}

  Status load(const DestinationConfig& config) override {
    if (config.count > StorageCapacity ||
        (config.count > 0 && config.entries == 0) ||
        config.count > config.capacity)
      return Status(ErrorCode::INVALID_ARGUMENT);

    Destination candidate[StorageCapacity];
    for (uint16_t i = 0; i < config.count; ++i) {
      const Destination& entry = config.entries[i];
      if (entry.id == 0 || !hasTerminator(entry.label))
        return Status(ErrorCode::INVALID_ARGUMENT);
      for (uint16_t j = 0; j < i; ++j)
        if (candidate[j].id == entry.id)
          return Status(ErrorCode::DUPLICATE);
      candidate[i] = entry;
    }

    for (uint16_t i = 0; i < config.count; ++i)
      entries_[i] = candidate[i];
    count_ = config.count;
    loaded_ = true;
    return Status();
  }

  uint16_t count() const override { return loaded_ ? count_ : 0; }

  bool getByIndex(uint16_t index, Destination& destination) const override {
    if (!loaded_ || index >= count_)
      return false;
    destination = entries_[index];
    return true;
  }

  bool findById(DeviceId id, Destination& destination) const override {
    if (!loaded_ || id == 0)
      return false;
    for (uint16_t i = 0; i < count_; ++i) {
      if (entries_[i].id == id) {
        destination = entries_[i];
        return true;
      }
    }
    return false;
  }

  bool isEnabled(DeviceId id) const override {
    Destination destination = {};
    return findById(id, destination) && destination.enabled;
  }

private:
  Destination entries_[StorageCapacity];
  uint16_t count_;
  bool loaded_;

  static bool hasTerminator(const char* label) {
    for (uint8_t i = 0; i <= kMaxDestinationLabelBytes; ++i)
      if (label[i] == '\0')
        return true;
    return false;
  }
};

} // namespace ca
