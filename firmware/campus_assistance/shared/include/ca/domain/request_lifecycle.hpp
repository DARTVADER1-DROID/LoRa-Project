#pragma once

#include "ca/types.hpp"

namespace ca {

class RequestLifecycle {
public:
  virtual ~RequestLifecycle() {}
  virtual Status create(const NewRequest& input, RequestRecord& created) = 0;
  virtual Status cancel(const RequestId& id, CancellationSource source) = 0;
  virtual Status expireDue(uint32_t stationNowMs, ExpiredRequestSet& expired) = 0;
  virtual Status applyJourneyUpdate(const JourneyUpdate& update) = 0;
  virtual bool find(const RequestId& id, RequestRecord& record) const = 0;
};

} // namespace ca
