#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <string.h>

#include "ca/ports/byte_storage.hpp"

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#else
#include <EEPROM.h>
#endif

namespace ca {
namespace platform {

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

// Preferences stores the complete two-slot image as one NVS blob. Writes are
// staged in RAM and become durable only at sync(), preserving the previous NVS
// blob if a new blob commit fails.
template <uint16_t StorageBytes>
class Esp32PreferencesByteStorage final : public IByteStorage {
  static_assert(StorageBytes > 0, "StorageBytes must be nonzero");

public:
  Esp32PreferencesByteStorage()
      : initialized_(false), buffer_() {}

  Status begin() override {
    if (initialized_)
      return Status(ErrorCode::INVALID_STATE);
    if (!preferences_.begin("ca_storage", false))
      return Status(ErrorCode::STORAGE_FAILURE);

    const size_t storedLength = preferences_.getBytesLength("slots");
    if (storedLength == StorageBytes) {
      if (preferences_.getBytes("slots", buffer_, StorageBytes) != StorageBytes)
        return Status(ErrorCode::STORAGE_FAILURE);
    } else {
      memset(buffer_, 0xFF, StorageBytes);
    }
    initialized_ = true;
    return Status();
  }

  uint16_t capacity() const override { return StorageBytes; }

  Status read(uint16_t offset, uint8_t* output, uint16_t length) const override {
    if (!initialized_ || (length > 0 && output == 0) ||
        static_cast<uint32_t>(offset) + length > StorageBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i)
      output[i] = buffer_[offset + i];
    return Status();
  }

  Status write(uint16_t offset, const uint8_t* data, uint16_t length) override {
    if (!initialized_ || (length > 0 && data == 0) ||
        static_cast<uint32_t>(offset) + length > StorageBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i)
      buffer_[offset + i] = data[i];
    return Status();
  }

  Status sync() override {
    if (!initialized_)
      return Status(ErrorCode::INVALID_STATE);
    return preferences_.putBytes("slots", buffer_, StorageBytes) == StorageBytes
               ? Status()
               : Status(ErrorCode::STORAGE_FAILURE);
  }

private:
  Preferences preferences_;
  bool initialized_;
  uint8_t buffer_[StorageBytes];
};

#else

// AVR EEPROM writes are byte-granular. FixedDualSlotPersistence alternates
// records and writes the commit marker last, leaving the previous slot intact.
template <uint16_t StorageBytes>
class ArduinoUnoEepromByteStorage final : public IByteStorage {
  static_assert(StorageBytes > 0, "StorageBytes must be nonzero");

public:
  explicit ArduinoUnoEepromByteStorage(uint16_t baseAddress = 0)
      : baseAddress_(baseAddress), initialized_(false) {}

  Status begin() override {
    if (initialized_ ||
        static_cast<uint32_t>(baseAddress_) + StorageBytes >
            static_cast<uint16_t>(EEPROM.length()))
      return Status(ErrorCode::CONFIGURATION_INVALID);
    initialized_ = true;
    return Status();
  }

  uint16_t capacity() const override { return StorageBytes; }

  Status read(uint16_t offset, uint8_t* output, uint16_t length) const override {
    if (!initialized_ || (length > 0 && output == 0) ||
        static_cast<uint32_t>(offset) + length > StorageBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i)
      output[i] = EEPROM.read(static_cast<int>(baseAddress_ + offset + i));
    return Status();
  }

  Status write(uint16_t offset, const uint8_t* data, uint16_t length) override {
    if (!initialized_ || (length > 0 && data == 0) ||
        static_cast<uint32_t>(offset) + length > StorageBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);
    for (uint16_t i = 0; i < length; ++i)
      EEPROM.update(static_cast<int>(baseAddress_ + offset + i), data[i]);
    return Status();
  }

  Status sync() override { return initialized_ ? Status() : Status(ErrorCode::INVALID_STATE); }

private:
  uint16_t baseAddress_;
  bool initialized_;
};

#endif

} // namespace platform
} // namespace ca

#endif // ARDUINO
