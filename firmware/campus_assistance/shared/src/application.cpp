#include "../include/ca/application.hpp"
namespace ca {
Status StationApplication::begin(const DeviceConfig& c){/* TODO: validate STATION config and initialize services; create no request. */(void)c;return Status(ErrorCode::NOT_IMPLEMENTED);}
void StationApplication::service(uint32_t now){/* TODO: bounded station loop: inputs, radio, lifecycle, TX, UI, diagnostics. */(void)now;}
void StationApplication::onButtonEvent(const ButtonEvent& e){/* TODO: convert to validated station intent. */(void)e;}
void StationApplication::onProtocolMessage(const ProtocolMessage& m){/* TODO: dispatch validated CLAIM and journey events. */(void)m;}
bool StationApplication::isHealthy()const{/* TODO: report readiness only after required services pass. */return false;}
Status CartApplication::begin(const DeviceConfig& c){/* TODO: validate CART config; initialize receiver; boot NOT_ACCEPTING. */(void)c;return Status(ErrorCode::NOT_IMPLEMENTED);}
void CartApplication::service(uint32_t now){/* TODO: continuously service radio; claim only when driver explicitly eligible. */(void)now;}
void CartApplication::onButtonEvent(const ButtonEvent& e){/* TODO: validate driver enable/disable/progress actions. */(void)e;}
void CartApplication::onProtocolMessage(const ProtocolMessage& m){/* TODO: receive REQUEST in every mode; reconcile ASSIGN/CANCEL/EXPIRED. */(void)m;}
DriverMode CartApplication::driverMode()const{/* TODO: return driver controller state. */return DriverMode::NOT_ACCEPTING;}
bool CartApplication::hasActiveAssignment()const{/* TODO: report station-confirmed active assignment. */return false;}
}
