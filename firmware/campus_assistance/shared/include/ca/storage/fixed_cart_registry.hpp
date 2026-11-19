#pragma once

#include "ca/ports/network.hpp"

namespace ca {

// Fixed-capacity validated roster used by station-side reliable broadcasts.
template <uint16_t StorageCapacity>
class FixedCartRegistry final : public ICartRegistry {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedCartRegistry() : count_(0), loaded_(false) {}

  Status load(const CartRegistryView& view) {
    if (loaded_ || view.count > view.capacity ||
        (view.count > 0 && view.entries == 0) ||
        view.count > StorageCapacity)
      return Status(ErrorCode::INVALID_ARGUMENT);

    uint16_t nextCount = 0;
    for (uint16_t i = 0; i < view.count; ++i) {
      const CartRegistryEntry& entry = view.entries[i];
      if (!entry.enabled)
        continue;
      if (entry.cartId == 0)
        return Status(ErrorCode::INVALID_ARGUMENT);
      for (uint16_t j = 0; j < nextCount; ++j)
        if (cartIds_[j] == entry.cartId)
          return Status(ErrorCode::DUPLICATE);
      cartIds_[nextCount++] = entry.cartId;
    }

    if (nextCount == 0)
      return Status(ErrorCode::NOT_FOUND);
    count_ = nextCount;
    loaded_ = true;
    return Status();
  }

  uint16_t enabledCount() const override { return loaded_ ? count_ : 0; }

  DeviceId enabledCartAt(uint16_t index) const override {
    return loaded_ && index < count_ ? cartIds_[index] : 0;
  }

private:
  DeviceId cartIds_[StorageCapacity];
  uint16_t count_;
  bool loaded_;
};

} // namespace ca
