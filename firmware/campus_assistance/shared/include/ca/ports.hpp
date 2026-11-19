#pragma once

// Convenience aggregate for consumers that intentionally need every port.
// Internal layers should include only the narrow port headers they use.
#include "ca/ports/hardware.hpp"
#include "ca/ports/persistence.hpp"
#include "ca/ports/request_repository.hpp"
#include "ca/ports/runtime.hpp"
