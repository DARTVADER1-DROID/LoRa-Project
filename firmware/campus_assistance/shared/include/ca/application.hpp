#pragma once
#include "types.hpp"
namespace ca {
class StationApplication{public:Status begin(const DeviceConfig&);void service(uint32_t);void onButtonEvent(const ButtonEvent&);void onProtocolMessage(const ProtocolMessage&);bool isHealthy()const;};
class CartApplication{public:Status begin(const DeviceConfig&);void service(uint32_t);void onButtonEvent(const ButtonEvent&);void onProtocolMessage(const ProtocolMessage&);DriverMode driverMode()const;bool hasActiveAssignment()const;};
}
