#pragma once

#include "ca/types.hpp"

namespace ca {

class RequestIdentity {
public:
  static bool isValid(const RequestId& id);
  static bool equals(const RequestId& lhs, const RequestId& rhs);
  static uint32_t hash(const RequestId& id);
};

} // namespace ca
