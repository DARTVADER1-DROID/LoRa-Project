#pragma once

#include <stdint.h>

namespace ca {

static const uint32_t kDefaultRequestLifetimeMs = 600000UL;
static const uint16_t kMaxProtocolPayloadBytes = 64;
static const uint16_t kMaxFrameBytes = 96;
static const uint8_t kMaxPhysicalButtons = 2;

typedef uint16_t DeviceId;
typedef uint16_t NetworkId;
typedef uint32_t RequestSequence;
typedef uint16_t RequestGeneration;
typedef uint32_t TransactionId;
typedef uint16_t AssignmentEpoch;
typedef uint32_t EventSequence;
typedef uint16_t RecordKey;

enum class DeviceRole : uint8_t { STATION, CART };
enum class DriverMode : uint8_t {
  NOT_ACCEPTING, ACCEPTING_IDLE, CLAIMING, SERVING, UNCERTAIN
};
enum class RequestState : uint8_t {
  CREATED, PENDING_UNASSIGNED, CLAIM_PENDING, ASSIGNED, ON_THE_WAY,
  ARRIVED_AT_PICKUP, COMPLETED, CANCELLED, EXPIRED,
  COMMUNICATION_UNCERTAIN
};
enum class JourneyStatus : uint8_t {
  RECEIVED, ON_THE_WAY, ARRIVED_AT_PICKUP, COMPLETED
};
enum class ButtonEventType : uint8_t { SHORT_PRESS, LONG_PRESS, RELEASE };
enum class DriverActionType : uint8_t {
  ENABLE_ACCEPTANCE, DISABLE_ACCEPTANCE, ADVANCE_JOURNEY, CONFIRM_COMPLETION
};
enum class CancellationSource : uint8_t { STUDENT, STATION_OPERATOR, SYSTEM };
enum class DeliveryClass : uint8_t { BEST_EFFORT, RETRY_UNTIL_ACK, PRIORITY };
enum class MessageType : uint8_t {
  REQUEST, CLAIM, CLAIM_WITHDRAW, CLAIM_RESULT, ASSIGN, CANCEL, EXPIRED, JOURNEY_UPDATE,
  SNAPSHOT_QUERY, SNAPSHOT, ACKNOWLEDGEMENT
};
enum class ProtocolEventType : uint8_t {
  REQUEST_RECEIVED, CLAIM_RECEIVED, CLAIM_WITHDRAW_RECEIVED, CLAIM_RESULT_RECEIVED, ASSIGNMENT_RECEIVED,
  CANCELLATION_RECEIVED, EXPIRY_RECEIVED, JOURNEY_UPDATE_RECEIVED,
  SNAPSHOT_QUERY_RECEIVED, SNAPSHOT_RECEIVED, ACKNOWLEDGEMENT_RECEIVED
};
enum class ErrorCode : uint8_t {
  NONE, INVALID_ARGUMENT, INVALID_STATE, NOT_FOUND, DUPLICATE, EXPIRED,
  CANCELLED, ALREADY_ASSIGNED, NOT_ACCEPTING, BUSY, QUEUE_FULL, BUFFER_FULL,
  TIMEOUT, RADIO_UNAVAILABLE, PROTOCOL_MISMATCH, STORAGE_FAILURE,
  CONFIGURATION_INVALID, COMMUNICATION_UNCERTAIN, STALE_EVENT,
  CAPACITY_EXCEEDED, NOT_IMPLEMENTED
};

struct Status {
  ErrorCode code;
  explicit Status(ErrorCode value = ErrorCode::NONE) : code(value) {}
  bool ok() const { return code == ErrorCode::NONE; }
};

struct RequestId {
  DeviceId originStationId;
  RequestSequence sequence;
  RequestGeneration generation;
};

struct NewRequest {
  DeviceId destinationId;
};

struct RequestRecord {
  RequestId id;
  DeviceId destinationId;
  uint32_t createdAtMs;
  uint32_t expiresAtMs;
  RequestState state;
  DeviceId assignedCartId;
  AssignmentEpoch assignmentEpoch;
  EventSequence lastJourneyEventSequence;
};

struct RequestSummary {
  RequestId id;
  DeviceId destinationId;
  uint32_t localAgeMs;
};

struct AssignmentRecord {
  RequestId requestId;
  DeviceId assignedCartId;
  AssignmentEpoch epoch;
  RequestState state;
};

struct ClaimRequest {
  RequestId requestId;
  DeviceId cartId;
  TransactionId transactionId;
};

struct ClaimResult {
  RequestId requestId;
  TransactionId transactionId;
  ErrorCode result;
};

struct AssignmentNotice {
  AssignmentRecord assignment;
};

struct CancellationNotice {
  RequestId requestId;
  CancellationSource source;
};

struct ExpiryNotice {
  RequestId requestId;
};

struct JourneyUpdate {
  RequestId requestId;
  DeviceId assignedCartId;
  JourneyStatus status;
  EventSequence eventSequence;
};

struct RequestSnapshot {
  RequestId requestId;
  RequestState state;
  DeviceId assignedCartId;
  AssignmentEpoch assignmentEpoch;
  EventSequence lastEventSequence;
};

struct DriverAction {
  DriverActionType type;
  uint32_t timestampMs;
};

struct ButtonEvent {
  uint8_t buttonIndex;
  ButtonEventType type;
  uint32_t timestampMs;
};

struct RadioProfile {
  uint32_t frequencyHz;
  uint8_t spreadingFactor;
  uint32_t bandwidthHz;
  uint8_t codingRate;
  uint8_t transmitPowerDbm;
};

struct DeviceConfig {
  DeviceRole role;
  DeviceId deviceId;
  NetworkId networkId;
  uint8_t protocolVersion;
  uint16_t radioProfileId;
  uint16_t boardProfileId;
  uint16_t pendingCapacity;
  uint32_t requestLifetimeMs;
};

struct ProtocolHeader {
  uint8_t protocolVersion;
  NetworkId networkId;
  MessageType messageType;
  DeviceId sourceDeviceId;
  DeviceId destinationDeviceId;
  TransactionId transactionId;
  uint32_t messageSequence;
};

struct ProtocolMessage {
  ProtocolHeader header;
  uint16_t payloadLength;
  uint8_t payload[kMaxProtocolPayloadBytes];
};

struct ReceivedFrame {
  uint16_t length;
  uint8_t bytes[kMaxFrameBytes];
};

struct FrameView {
  const uint8_t* data;
  uint16_t length;
};

struct MutableFrameView {
  uint8_t* data;
  uint16_t capacity;
};

struct RadioHealth {
  bool initialized;
  bool receiving;
  bool faulted;
};

struct ButtonConfig {
  uint8_t buttonCount;
  uint16_t debounceMs;
  uint16_t longPressMs;
};

struct BuzzerConfig {
  uint16_t defaultToneHz;
  uint16_t defaultDurationMs;
};

struct UiViewModel {
  char line1[17];
  char line2[17];
  uint8_t screenId;
};

struct ExpiredRequestSet {
  uint16_t count;
  RequestId* items;
  uint16_t capacity;
};

struct StationRegistry;
struct DestinationConfig;
struct Destination;

}  // namespace ca
