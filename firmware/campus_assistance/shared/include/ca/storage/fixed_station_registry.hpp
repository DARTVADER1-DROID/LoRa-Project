#pragma once

#include "ca/ports/network.hpp"

namespace ca {

template <uint16_t StorageCapacity>
class FixedStationRegistry final : public IStationRegistry {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");

public:
  FixedStationRegistry() : count_(0), loaded_(false) {}

  Status load(const StationRegistry& view) {
    if (loaded_ || view.count > view.capacity ||
        (view.count > 0 && view.entries == 0) ||
        view.count > StorageCapacity)
      return Status(ErrorCode::INVALID_ARGUMENT);

    uint16_t nextCount = 0;
    for (uint16_t i = 0; i < view.count; ++i) {
      const StationRegistryEntry& entry = view.entries[i];
      if (!entry.enabled)
        continue;
      if (entry.stationId == 0)
        return Status(ErrorCode::INVALID_ARGUMENT);
      for (uint16_t j = 0; j < nextCount; ++j)
        if (stationIds_[j] == entry.stationId)
          return Status(ErrorCode::DUPLICATE);
      stationIds_[nextCount++] = entry.stationId;
    }

    if (nextCount == 0)
      return Status(ErrorCode::NOT_FOUND);
    count_ = nextCount;
    loaded_ = true;
    return Status();
  }

  bool containsStation(DeviceId stationId) const override {
    if (!loaded_ || stationId == 0)
      return false;
    for (uint16_t i = 0; i < count_; ++i)
      if (stationIds_[i] == stationId)
        return true;
    return false;
  }

private:
  DeviceId stationIds_[StorageCapacity];
  uint16_t count_;
  bool loaded_;
};

} // namespace ca
