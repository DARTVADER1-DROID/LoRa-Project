#pragma once
#include <stdint.h>
namespace ca {
enum class DeviceRole:uint8_t{STATION,CART};
enum class DriverMode:uint8_t{NOT_ACCEPTING,ACCEPTING_IDLE,CLAIMING,SERVING,UNCERTAIN};
enum class RequestState:uint8_t{CREATED,PENDING_UNASSIGNED,CLAIM_PENDING,ASSIGNED,ON_THE_WAY,ARRIVED_AT_PICKUP,COMPLETED,CANCELLED,EXPIRED,COMMUNICATION_UNCERTAIN};
enum class JourneyStatus:uint8_t{RECEIVED,ON_THE_WAY,ARRIVED_AT_PICKUP,COMPLETED};
enum class ErrorCode:uint8_t{NONE,INVALID_ARGUMENT,INVALID_STATE,NOT_FOUND,DUPLICATE,EXPIRED,CANCELLED,ALREADY_ASSIGNED,NOT_ACCEPTING,BUSY,QUEUE_FULL,BUFFER_FULL,TIMEOUT,RADIO_UNAVAILABLE,PROTOCOL_MISMATCH,STORAGE_FAILURE,CONFIGURATION_INVALID,COMMUNICATION_UNCERTAIN,NOT_IMPLEMENTED};
struct Status{ErrorCode code;explicit Status(ErrorCode c=ErrorCode::NONE):code(c){} bool ok()const{return code==ErrorCode::NONE;}};
typedef uint16_t DeviceId; typedef uint16_t NetworkId; typedef uint32_t RequestSequence; typedef uint16_t RequestGeneration; typedef uint32_t TransactionId; typedef uint16_t AssignmentEpoch;
struct RequestId{DeviceId originStationId;RequestSequence sequence;RequestGeneration generation;};
struct RequestSummary{RequestId id;DeviceId destinationId;uint32_t localAgeMs;};
struct AssignmentRecord{RequestId requestId;DeviceId assignedCartId;AssignmentEpoch epoch;RequestState state;};
struct ButtonEvent{uint8_t buttonIndex;uint8_t eventType;uint32_t timestampMs;};
struct RadioProfile{uint32_t frequencyHz;uint8_t spreadingFactor;uint32_t bandwidthHz;uint8_t codingRate;uint8_t transmitPowerDbm;};
struct DeviceConfig{DeviceRole role;DeviceId deviceId;NetworkId networkId;uint8_t protocolVersion;RadioProfile radioProfile;uint16_t pendingCapacity;uint32_t requestLifetimeMs;};
struct NewRequest{DeviceId destinationId;};
struct ClaimRequest{RequestId requestId;DeviceId cartId;TransactionId transactionId;};
struct AssignmentNotice{AssignmentRecord assignment;};
struct ProtocolMessage{uint8_t messageType;DeviceId sourceDeviceId;NetworkId networkId;RequestId requestId;uint16_t payloadLength;uint8_t payload[64];};
struct ReceivedFrame{uint16_t length;uint8_t bytes[96];};
struct UiViewModel{char line1[17];char line2[17];uint8_t screenId;};
enum class StatusCode:uint8_t{OK,REJECTED,PENDING,NOT_IMPLEMENTED};
}
