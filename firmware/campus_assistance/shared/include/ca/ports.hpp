#pragma once

#include "types.hpp"

namespace ca {

class IRadioTransport {
 public:
  virtual ~IRadioTransport() {}
  virtual Status begin(const RadioProfile& profile) = 0;
  virtual RadioHealth health() const = 0;
  virtual Status send(FrameView frame) = 0;
  virtual bool pollReceived(ReceivedFrame& frame) = 0;
  virtual void service() = 0;
};

class IButtonInput {
 public:
  virtual ~IButtonInput() {}
  virtual Status begin(const ButtonConfig& config) = 0;
  virtual void service(uint32_t nowMs) = 0;
  virtual bool pollEvent(ButtonEvent& event) = 0;
};

class IDisplay {
 public:
  virtual ~IDisplay() {}
  virtual Status begin() = 0;
  virtual void render(const UiViewModel& view) = 0;
};

class IBuzzer {
 public:
  virtual ~IBuzzer() {}
  virtual Status begin(const BuzzerConfig& config) = 0;
  virtual void playCue(uint8_t cueId) = 0;
  virtual void service(uint32_t nowMs) = 0;
};

class IClock {
 public:
  virtual ~IClock() {}
  virtual uint32_t millisNow() const = 0;
};

class IPersistence {
 public:
  virtual ~IPersistence() {}
  virtual Status load(RecordKey key, uint8_t* output, uint16_t capacity) = 0;
  virtual Status storeAtomically(RecordKey key, const uint8_t* data,
                                 uint16_t length) = 0;
  virtual Status erase(RecordKey key) = 0;
};

class IDiagnostics {
 public:
  virtual ~IDiagnostics() {}
  virtual void record(uint16_t eventId, ErrorCode error) = 0;
  virtual void increment(uint16_t counterId) = 0;
};

class IRequestRepository {
 public:
  virtual ~IRequestRepository() {}
  virtual Status insert(const RequestRecord& record) = 0;
  virtual bool find(const RequestId& id, RequestRecord& record) const = 0;
  virtual Status update(const RequestRecord& record) = 0;
  // Must atomically assign only while the stored request is still unassigned
  // and non-terminal; this is the persistence-level single-winner guard.
  virtual Status assignIfUnassigned(const RequestId& id, DeviceId cartId,
                                    AssignmentEpoch epoch, uint32_t stationNowMs,
                                    RequestRecord& updated) = 0;
  virtual Status removeTerminalBefore(uint32_t retentionBoundaryMs) = 0;
  virtual uint16_t size() const = 0;
};

}  // namespace ca
