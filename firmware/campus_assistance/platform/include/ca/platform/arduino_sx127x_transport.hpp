#pragma once

#ifdef ARDUINO

#include <Arduino.h>
#include <SPI.h>

#include "ca/platform/board_profile.hpp"
#include "ca/ports/hardware.hpp"

namespace ca {
namespace platform {

// Poll-driven SX1276/SX1278 LoRa-mode transport. It uses the Arduino SPI
// abstraction and injected pin map, so the same adapter can serve Uno and
// ESP32-class boards. No RadioLib/LoRa library dependency is required.
class ArduinoSx1278Transport final : public IRadioTransport {
public:
  ArduinoSx1278Transport(const BoardPinMap& pins, SPIClass& spi,
                         uint32_t spiClockHz = 8000000UL)
      : pins_(pins), spi_(spi), spiClockHz_(spiClockHz), initialized_(false),
        transmitting_(false), faulted_(false), txStartedAtMs_(0),
        receiveHead_(0), receiveCount_(0) {}

  Status begin(const RadioProfile& profile) override {
    if (initialized_ || !isValidPinMap(pins_) || spiClockHz_ == 0 ||
        spiClockHz_ > 10000000UL || profile.frequencyHz < 410000000UL ||
        profile.frequencyHz > 525000000UL || profile.spreadingFactor < 7 ||
        profile.spreadingFactor > 12 || profile.codingRate < 1 ||
        profile.codingRate > 4 || profile.transmitPowerDbm < 2 ||
        profile.transmitPowerDbm > 17 || bandwidthCode(profile.bandwidthHz) < 0)
      return Status(ErrorCode::CONFIGURATION_INVALID);

    pinMode(pins_.radioChipSelect, OUTPUT);
    digitalWrite(pins_.radioChipSelect, HIGH);
    pinMode(pins_.radioReset, OUTPUT);
    pinMode(pins_.radioDio0, INPUT);

#if defined(ESP32)
    spi_.begin(pins_.radioSck, pins_.radioMiso, pins_.radioMosi,
               pins_.radioChipSelect);
#else
    spi_.begin();
#endif

    digitalWrite(pins_.radioReset, LOW);
    delay(10);
    digitalWrite(pins_.radioReset, HIGH);
    delay(10);

    if (readRegister(kRegVersion) != 0x12)
      return Status(ErrorCode::RADIO_UNAVAILABLE);

    writeRegister(kRegOpMode, kModeSleep);
    delay(2);
    writeRegister(kRegOpMode, kModeStandby);

    const uint64_t frf =
        (static_cast<uint64_t>(profile.frequencyHz) << 19) / 32000000ULL;
    writeRegister(kRegFrfMsb, static_cast<uint8_t>(frf >> 16));
    writeRegister(kRegFrfMid, static_cast<uint8_t>(frf >> 8));
    writeRegister(kRegFrfLsb, static_cast<uint8_t>(frf));

    writeRegister(kRegPaConfig,
                  static_cast<uint8_t>(0x80U | (profile.transmitPowerDbm - 2U)));
    writeRegister(kRegPaDac, 0x84);
    writeRegister(kRegOcp, 0x2B);
    writeRegister(kRegLna, 0x23);
    writeRegister(kRegFifoTxBase, 0x00);
    writeRegister(kRegFifoRxBase, 0x00);
    writeRegister(kRegMaxPayloadLength, kMaxFrameBytes);
    writeRegister(kRegPreambleMsb, 0x00);
    writeRegister(kRegPreambleLsb, 0x08);
    writeRegister(kRegSyncWord, 0x12);

    const uint8_t bandwidth = static_cast<uint8_t>(bandwidthCode(profile.bandwidthHz));
    const uint8_t modemConfig1 =
        static_cast<uint8_t>((bandwidth << 4) | (profile.codingRate << 1));
    const uint8_t modemConfig2 =
        static_cast<uint8_t>((profile.spreadingFactor << 4) | 0x04U);
    const bool lowDataRateOptimize =
        (static_cast<uint64_t>(1UL << profile.spreadingFactor) * 1000000ULL) >
        (static_cast<uint64_t>(profile.bandwidthHz) * 16000ULL);
    const uint8_t modemConfig3 =
        static_cast<uint8_t>(0x04U | (lowDataRateOptimize ? 0x08U : 0U));

    writeRegister(kRegModemConfig1, modemConfig1);
    writeRegister(kRegModemConfig2, modemConfig2);
    writeRegister(kRegModemConfig3, modemConfig3);
    writeRegister(kRegDioMapping1, 0x00);
    writeRegister(kRegIrqFlags, 0xFF);
    writeRegister(kRegOpMode, kModeReceiveContinuous);

    profile_ = profile;
    initialized_ = true;
    transmitting_ = false;
    faulted_ = false;
    return Status();
  }

  RadioHealth health() const override {
    RadioHealth result = {initialized_, initialized_ && !transmitting_ && !faulted_,
                          faulted_};
    return result;
  }

  Status send(FrameView frame) override {
    if (!initialized_ || faulted_)
      return Status(ErrorCode::RADIO_UNAVAILABLE);
    if (transmitting_)
      return Status(ErrorCode::BUSY);
    if (frame.data == 0 || frame.length == 0 || frame.length > kMaxFrameBytes)
      return Status(ErrorCode::INVALID_ARGUMENT);

    writeRegister(kRegOpMode, kModeStandby);
    writeRegister(kRegFifoAddrPtr, 0x00);
    writeRegister(kRegPayloadLength, static_cast<uint8_t>(frame.length));
    writeFifo(frame.data, frame.length);
    writeRegister(kRegIrqFlags, 0xFF);
    writeRegister(kRegOpMode, kModeTransmit);
    txStartedAtMs_ = millis();
    transmitting_ = true;
    return Status();
  }

  bool pollReceived(ReceivedFrame& frame) override {
    if (receiveCount_ == 0)
      return false;
    frame = received_[receiveHead_];
    receiveHead_ = static_cast<uint8_t>((receiveHead_ + 1U) % kReceiveCapacity);
    --receiveCount_;
    return true;
  }

  void service() override {
    if (!initialized_ || faulted_)
      return;

    if (transmitting_) {
      const uint8_t flags = readRegister(kRegIrqFlags);
      if ((flags & kIrqTxDone) != 0) {
        writeRegister(kRegIrqFlags, kIrqTxDone);
        transmitting_ = false;
        writeRegister(kRegOpMode, kModeReceiveContinuous);
      } else if (hasElapsed(millis(), txStartedAtMs_, kTransmitTimeoutMs)) {
        faulted_ = true;
        transmitting_ = false;
      }
      return;
    }

    if (digitalRead(pins_.radioDio0) == LOW)
      return;

    const uint8_t flags = readRegister(kRegIrqFlags);
    if ((flags & kIrqRxDone) == 0)
      return;

    if ((flags & kIrqPayloadCrcError) != 0) {
      writeRegister(kRegIrqFlags, kIrqRxDone | kIrqPayloadCrcError);
      writeRegister(kRegOpMode, kModeReceiveContinuous);
      return;
    }

    const uint8_t length = readRegister(kRegRxNbBytes);
    if (length == 0 || length > kMaxFrameBytes || receiveCount_ >= kReceiveCapacity) {
      writeRegister(kRegIrqFlags, 0xFF);
      writeRegister(kRegOpMode, kModeReceiveContinuous);
      return;
    }

    writeRegister(kRegFifoAddrPtr, readRegister(kRegFifoRxCurrentAddr));
    const uint8_t tail = static_cast<uint8_t>(
        (receiveHead_ + receiveCount_) % kReceiveCapacity);
    received_[tail].length = length;
    readFifo(received_[tail].bytes, length);
    ++receiveCount_;

    writeRegister(kRegIrqFlags, 0xFF);
    writeRegister(kRegOpMode, kModeReceiveContinuous);
  }

private:
  static const uint8_t kReceiveCapacity = 2;
  static const uint32_t kTransmitTimeoutMs = 10000UL;

  static const uint8_t kRegFifo = 0x00;
  static const uint8_t kRegOpMode = 0x01;
  static const uint8_t kRegFrfMsb = 0x06;
  static const uint8_t kRegFrfMid = 0x07;
  static const uint8_t kRegFrfLsb = 0x08;
  static const uint8_t kRegPaConfig = 0x09;
  static const uint8_t kRegOcp = 0x0B;
  static const uint8_t kRegLna = 0x0C;
  static const uint8_t kRegFifoAddrPtr = 0x0D;
  static const uint8_t kRegFifoTxBase = 0x0E;
  static const uint8_t kRegFifoRxBase = 0x0F;
  static const uint8_t kRegFifoRxCurrentAddr = 0x10;
  static const uint8_t kRegIrqFlags = 0x12;
  static const uint8_t kRegRxNbBytes = 0x13;
  static const uint8_t kRegModemConfig1 = 0x1D;
  static const uint8_t kRegModemConfig2 = 0x1E;
  static const uint8_t kRegPreambleMsb = 0x20;
  static const uint8_t kRegPreambleLsb = 0x21;
  static const uint8_t kRegPayloadLength = 0x22;
  static const uint8_t kRegMaxPayloadLength = 0x23;
  static const uint8_t kRegModemConfig3 = 0x26;
  static const uint8_t kRegSyncWord = 0x39;
  static const uint8_t kRegDioMapping1 = 0x40;
  static const uint8_t kRegVersion = 0x42;
  static const uint8_t kRegPaDac = 0x4D;

  static const uint8_t kModeSleep = 0x80;
  static const uint8_t kModeStandby = 0x81;
  static const uint8_t kModeTransmit = 0x83;
  static const uint8_t kModeReceiveContinuous = 0x85;
  static const uint8_t kIrqRxDone = 0x40;
  static const uint8_t kIrqPayloadCrcError = 0x20;
  static const uint8_t kIrqTxDone = 0x08;

  BoardPinMap pins_;
  SPIClass& spi_;
  uint32_t spiClockHz_;
  RadioProfile profile_;
  bool initialized_;
  bool transmitting_;
  bool faulted_;
  uint32_t txStartedAtMs_;
  ReceivedFrame received_[kReceiveCapacity];
  uint8_t receiveHead_;
  uint8_t receiveCount_;

  static int8_t bandwidthCode(uint32_t bandwidthHz) {
    switch (bandwidthHz) {
    case 7800UL:
      return 0;
    case 10400UL:
      return 1;
    case 15600UL:
      return 2;
    case 20800UL:
      return 3;
    case 31250UL:
      return 4;
    case 41700UL:
      return 5;
    case 62500UL:
      return 6;
    case 125000UL:
      return 7;
    case 250000UL:
      return 8;
    case 500000UL:
      return 9;
    default:
      return -1;
    }
  }

  uint8_t readRegister(uint8_t address) {
    spi_.beginTransaction(SPISettings(spiClockHz_, MSBFIRST, SPI_MODE0));
    digitalWrite(pins_.radioChipSelect, LOW);
    spi_.transfer(static_cast<uint8_t>(address & 0x7FU));
    const uint8_t value = spi_.transfer(0x00);
    digitalWrite(pins_.radioChipSelect, HIGH);
    spi_.endTransaction();
    return value;
  }

  void writeRegister(uint8_t address, uint8_t value) {
    spi_.beginTransaction(SPISettings(spiClockHz_, MSBFIRST, SPI_MODE0));
    digitalWrite(pins_.radioChipSelect, LOW);
    spi_.transfer(static_cast<uint8_t>(address | 0x80U));
    spi_.transfer(value);
    digitalWrite(pins_.radioChipSelect, HIGH);
    spi_.endTransaction();
  }

  void writeFifo(const uint8_t* data, uint16_t length) {
    spi_.beginTransaction(SPISettings(spiClockHz_, MSBFIRST, SPI_MODE0));
    digitalWrite(pins_.radioChipSelect, LOW);
    spi_.transfer(static_cast<uint8_t>(kRegFifo | 0x80U));
    for (uint16_t i = 0; i < length; ++i)
      spi_.transfer(data[i]);
    digitalWrite(pins_.radioChipSelect, HIGH);
    spi_.endTransaction();
  }

  void readFifo(uint8_t* data, uint16_t length) {
    spi_.beginTransaction(SPISettings(spiClockHz_, MSBFIRST, SPI_MODE0));
    digitalWrite(pins_.radioChipSelect, LOW);
    spi_.transfer(kRegFifo);
    for (uint16_t i = 0; i < length; ++i)
      data[i] = spi_.transfer(0x00);
    digitalWrite(pins_.radioChipSelect, HIGH);
    spi_.endTransaction();
  }
};

} // namespace platform
} // namespace ca

#endif // ARDUINO
