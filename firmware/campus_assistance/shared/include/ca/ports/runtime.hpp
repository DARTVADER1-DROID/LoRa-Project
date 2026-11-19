#pragma once

#include "ca/types.hpp"

namespace ca {

class IClock {
public:
  virtual ~IClock() {}
  virtual uint32_t millisNow() const = 0;
};

class IDiagnostics {
public:
  virtual ~IDiagnostics() {}
  virtual void record(uint16_t eventId, ErrorCode error) = 0;
  virtual void increment(uint16_t counterId) = 0;
};

} // namespace ca
