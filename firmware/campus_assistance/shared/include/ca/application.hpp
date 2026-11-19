#pragma once

#include "config.hpp"
#include "domain.hpp"
#include "protocol.hpp"
#include "station_request_controller.hpp"
#include "ui.hpp"

namespace ca {

// ProtocolEndpoint's concrete composition root must wire these four roles;
// the endpoint itself owns their bounded RX/TX/event orchestration.
struct ProtocolEndpointDependencies {
  FrameCodec& codec;
  MessageValidator& validator;
  IRadioTransport& transport;
  RetryManager& retries;
};

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
  ConfigValidator& configValidator;
  ButtonEventInterpreter& buttonInterpreter;
  ProtocolEndpoint& protocol;
  PendingRequestStore& pendingRequests;
  ActiveAssignment& activeAssignment;
  DriverAvailabilityController& driverAvailability;
  JourneyProgressController& journeyProgress;
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
  AcceptanceMode acceptanceMode() const;
  CartServiceState serviceState() const;
  bool hasActiveAssignment() const;

private:
  CartDependencies dependencies_;
  DeviceConfig config_;
  bool initialized_;
};

} // namespace ca
