# 01 — Architecture and Layers

## 1. Build targets

| Firmware image | Role | MCU |
|---|---|---|
| Station | Request origin and assignment authority | Arduino Uno |
| Cart | Request listener and driver console | Arduino Uno |
| Station | Request origin and assignment authority | ESP32 (board TBD) |
| Cart | Request listener and driver console | ESP32 (board TBD) |

Four build configurations share portable domain logic and one protocol definition. Station/cart role logic is separate; board-specific adapters differ.

## 2. Layer model

    ROLE APPLICATION
      StationApplication             CartApplication
             |                              |
    DOMAIN / USE CASES
      RequestLifecycle              DriverAvailability
      RequestLedger                 PendingRequestStore
      DestinationCatalog            ActiveAssignment
      StationClaimCoordinator       CartClaimClient
             |                              |
    PROTOCOL
      FrameCodec | MessageValidator | ProtocolEndpoint
      RetryManager | EventReconciler | RequestIdentity
             |
    SERVICES / PORTS
      IRadioTransport | IButtonInput | IDisplay | IBuzzer
      IClock | IPersistence | IDiagnostics
             |
    HARDWARE ADAPTERS
      SX1278 | GPIO | LCD | buzzer | nonvolatile storage
             |
    PLATFORM
      Uno adapter                         ESP32 adapter

## 3. Dependency rules

1. Domain logic depends only on domain value types and interfaces.
2. Protocol may depend on domain value types, never LCD/GPIO/board headers.
3. Applications orchestrate use cases; they do not manipulate SX1278 registers.
4. Hardware adapters implement portable interfaces.
5. Platform-specific APIs remain inside platform adapters.
6. Station-only modules are excluded from cart builds unless shared by contract, and vice versa.
7. Uno and ESP32 use the same codec and golden protocol test vectors.
8. UI rendering never mutates request ownership.
9. No blocking delay may prevent radio service or event processing.
10. No dependency on steering, braking, throttle or traction systems.

## 4. Module responsibilities

| Module | Owns | Must not own |
|---|---|---|
| StationApplication | Station use-case orchestration | Radio registers |
| CartApplication | Driver-gated cart orchestration | Global request truth |
| RequestLedger | Station authoritative lifecycle | UI rendering |
| PendingRequestStore | Cart-local FIFO view | Assignment authority |
| ActiveAssignment | Zero-or-one confirmed cart assignment | Pending queue |
| ClaimCoordinator | Station-side winner decision | Driver input |
| ProtocolCodec | Frame encoding/decoding | Business policy |
| RadioService | Frame transport | Request transitions |
| ButtonService | Debounced semantic events | Domain decisions |
| Display/Buzzer | User feedback | Request ownership |
| ConfigService | Validated provisioned settings | Runtime registration |
| PersistenceService | Durable records | Claim arbitration |
| DiagnosticsService | Counters/events | User-facing success claims |

## 5. Event-driven execution

Each bounded service iteration:
1. Collect hardware and radio events.
2. Validate raw input and convert to typed events.
3. Dispatch events to the role application.
4. Advance domain state machines.
5. Schedule outbound messages.
6. Update UI view models.
7. Flush bounded diagnostics/persistence work.

No function waits for a remote ACK or user input. Work exceeding a per-iteration budget is continued later.

## 6. Configuration contract

Conceptual immutable build configuration:

    struct DeviceConfig {
        DeviceRole role;
        DeviceId deviceId;
        NetworkId networkId;
        ProtocolVersion protocolVersion;
        RadioProfileId radioProfile;
        uint16_t pendingCapacity;
        uint32_t requestLifetimeMs; // default 600000
        BoardProfileId boardProfile;
    };

Required validation contracts:

    Status ConfigValidator::validate(const DeviceConfig&);
    Status ConfigValidator::validateStationRegistry(const StationRegistry&);
    Status ConfigValidator::validateRadioProfile(const RadioProfile&);

Reject duplicate/invalid IDs, unsupported combinations, zero capacities and invalid timing ranges before enabling application behavior.

## 7. Role composition

Station:
- StationApplication
  - StationRequestController
  - DestinationCatalog
  - RequestLifecycle / RequestLedger
  - StationClaimCoordinator
  - JourneyStatusTracker
  - StationUserInterface
  - ProtocolEndpoint

Cart:
- CartApplication
  - DriverAvailabilityController
  - PendingRequestStore
  - ActiveAssignment
  - CartClaimClient
  - JourneyProgressController
  - CartUserInterface
  - ProtocolEndpoint

## 8. Scalability rules

- New MCU: implement platform adapters; do not rewrite domain logic.
- New station/cart: provision a unique ID; do not create per-device classes.
- New destination: add registry data; do not create destination-specific code.
- New message: update versioning, validation, vectors and both role builds.
- Capacity scales only within measured RAM, airtime, regulatory and RF limits; no claim of unlimited nodes.
