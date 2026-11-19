#pragma once

#include "domain.hpp"
#include "protocol.hpp"
#include "station_request_controller.hpp"
#include "ui.hpp"

namespace ca {

// Composition roots receive collaborators explicitly. They own orchestration,
// not hardware construction, request truth, or protocol encoding.
struct StationDependencies {
  IClock& clock;
  IButtonInput& buttons;
  IDisplay& display;
  IBuzzer& buzzer;
  IDiagnostics& diagnostics;
  ConfigValidator& configValidator;
  ButtonEventInterpreter& buttonInterpreter;
  ProtocolEndpoint& protocol;
  StationRequestController& requestController;
  StationClaimCoordinator& claims;
  StationUserInterface& ui;
};

struct CartDependencies {
  IClock& clock;
  IButtonInput& buttons;
  IDisplay& display;
  IBuzzer& buzzer;
  IDiagnostics& diagnostics;
  ProtocolEndpoint& protocol;
  PendingRequestStore& pendingRequests;
  ActiveAssignment& activeAssignment;
  DriverAvailabilityController& driverAvailability;
  CartClaimClient& claimClient;
  EventReconciler& reconciler;
  CartUserInterface& ui;
};

class StationApplication {
 public:
  explicit StationApplication(StationDependencies dependencies);
  Status begin(const DeviceConfig& config);
  void service();
  void onButtonEvent(const ButtonEvent& event);
  void onProtocolEvent(const ProtocolEvent& event);
  bool isHealthy() const;

 private:
  StationDependencies dependencies_;
  DeviceConfig config_;
  bool initialized_;
};

class CartApplication {
 public:
  explicit CartApplication(CartDependencies dependencies);
  Status begin(const DeviceConfig& config);
  void service();
  void onButtonEvent(const ButtonEvent& event);
  void onProtocolEvent(const ProtocolEvent& event);
  DriverMode driverMode() const;
  bool hasActiveAssignment() const;

 private:
  CartDependencies dependencies_;
  DeviceConfig config_;
  bool initialized_;
};

}  // namespace ca
