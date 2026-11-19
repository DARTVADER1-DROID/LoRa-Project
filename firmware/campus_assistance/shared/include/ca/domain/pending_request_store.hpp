#pragma once

#include "ca/types.hpp"

namespace ca {

class PendingRequestStore {
public:
  virtual ~PendingRequestStore() {}
  virtual Status configure(uint16_t capacity) = 0;
  virtual Status insertIfAbsent(const RequestSummary& request) = 0;
  virtual bool peekOldest(RequestSummary& request) const = 0;
  virtual bool remove(const RequestId& id) = 0;
  virtual uint16_t removeExpired(const ExpiredRequestSet& expired) = 0;
  virtual uint16_t removeAssigned(const RequestId& id) = 0;
  virtual void clearLocal() = 0;
  virtual bool contains(const RequestId& id) const = 0;
  virtual uint16_t size() const = 0;
  virtual uint16_t capacity() const = 0;
};

} // namespace ca
