#pragma once

#include "ca/ports/persistence.hpp"
#include "ca/storage/fixed_request_repository.hpp"

namespace ca {

// A durable, bounded repository over an atomic-record persistence adapter.
// The adapter MUST guarantee old-or-new visibility for storeAtomically(), even
// across power loss. This class writes one versioned, checksummed snapshot.
// Capacity is selected per target after RAM/flash endurance measurements.
template <uint16_t StorageCapacity>
class DurableRequestRepository final : public IRequestRepository {
  static_assert(StorageCapacity > 0, "StorageCapacity must be nonzero");
  static_assert(
      StorageCapacity <= 1925,
      "Encoded snapshot length must fit the persistence API uint16_t length.");

  enum {
    kHeaderBytes = 17,
    kRecordBytes = 34,
    kChecksumBytes = 4,
    kMaxEncodedBytes = kHeaderBytes + (StorageCapacity * 34UL) + kChecksumBytes
  };

public:
  DurableRequestRepository(IPersistence& persistence, RecordKey key, DeviceId stationId)
      : persistence_(persistence), key_(key), stationId_(stationId), configured_(false),
        encodedLength_(0) {}

  Status configure() {
    if (configured_)
      return Status();

    if (stationId_ == 0 || key_ == 0)
      return Status(ErrorCode::INVALID_ARGUMENT);

    Status stateStatus = state_.configure();
    if (!stateStatus.ok())
      return stateStatus;

    uint16_t bytesRead = 0;
    const Status loaded =
        persistence_.load(key_, encoded_, kMaxEncodedBytes, bytesRead);

    if (loaded.code == ErrorCode::NOT_FOUND) {
      if (!persistSnapshot(state_))
        return Status(ErrorCode::STORAGE_FAILURE);
      configured_ = true;
      return Status();
    }
    if (!loaded.ok() || bytesRead > kMaxEncodedBytes || !decodeSnapshot(bytesRead))
      return Status(ErrorCode::STORAGE_FAILURE);

    FixedRequestRepository<StorageCapacity> recovered = state_;
    if (!recovered.expireUnassignedForRecovery().ok())
      return Status(ErrorCode::STORAGE_FAILURE);
    if (!sameState(state_, recovered)) {
      if (!persistSnapshot(recovered))
        return Status(ErrorCode::STORAGE_FAILURE);
      state_ = recovered;
    }

    configured_ = true;
    return Status();
  }

  Status reserveNextRequestId(DeviceId stationId, RequestId& id) override {
    if (!ready())
      return Status(ErrorCode::INVALID_STATE);
    if (stationId != stationId_)
      return Status(ErrorCode::INVALID_ARGUMENT);

    FixedRequestRepository<StorageCapacity> candidate = state_;
    RequestId reserved = {};
    const Status result = candidate.reserveNextRequestId(stationId, reserved);
    if (!result.ok())
      return result;
    if (!persistSnapshot(candidate))
      return Status(ErrorCode::STORAGE_FAILURE);

    state_ = candidate;
    id = reserved;
    return Status();
  }

  Status insertNew(const RequestRecord& record) override {
    if (!ready())
      return Status(ErrorCode::INVALID_STATE);
    if (record.id.originStationId != stationId_)
      return Status(ErrorCode::INVALID_ARGUMENT);
    FixedRequestRepository<StorageCapacity> candidate = state_;
    const Status result = candidate.insertNew(record);
    if (!result.ok())
      return result;
    if (!persistSnapshot(candidate))
      return Status(ErrorCode::STORAGE_FAILURE);
    state_ = candidate;
    return Status();
  }

  bool find(const RequestId& id, RequestRecord& record) const override {
    return ready() && state_.find(id, record);
  }

  bool recordAt(uint16_t index, RequestRecord& record) const override {
    return ready() && state_.recordAt(index, record);
  }

  Status beginClaimIfEligible(const RequestId& id,
                              DeviceId cartId,
                              TransactionId transactionId,
                              uint32_t now,
                              RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.beginClaimIfEligible(id, cartId, transactionId, now, result);
        },
        updated);
  }

  Status withdrawClaimIfPending(const RequestId& id,
                                DeviceId cartId,
                                TransactionId transactionId,
                                RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.withdrawClaimIfPending(id, cartId, transactionId, result);
        },
        updated);
  }

  Status commitAssignmentIfClaimPending(const RequestId& id,
                                        DeviceId cartId,
                                        TransactionId transactionId,
                                        AssignmentEpoch epoch,
                                        uint32_t now,
                                        RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.commitAssignmentIfClaimPending(
              id, cartId, transactionId, epoch, now, result);
        },
        updated);
  }

  Status cancelIfUnassigned(const RequestId& id,
                            CancellationSource source,
                            RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.cancelIfUnassigned(id, source, result);
        },
        updated);
  }

  Status
  expireIfDue(const RequestId& id, uint32_t now, RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.expireIfDue(id, now, result);
        },
        updated);
  }

  Status applyJourneyUpdateIfCurrent(const JourneyUpdate& update,
                                     RequestRecord& updated) override {
    return applyRecordTransition(
        [&](FixedRequestRepository<StorageCapacity>& candidate, RequestRecord& result) {
          return candidate.applyJourneyUpdateIfCurrent(update, result);
        },
        updated);
  }

  Status removeTerminalBefore(uint32_t retentionBoundaryMs) override {
    if (!ready())
      return Status(ErrorCode::INVALID_STATE);
    FixedRequestRepository<StorageCapacity> candidate = state_;
    const Status result = candidate.removeTerminalBefore(retentionBoundaryMs);
    if (!result.ok())
      return result;
    if (!persistSnapshot(candidate))
      return Status(ErrorCode::STORAGE_FAILURE);
    state_ = candidate;
    return Status();
  }

  uint16_t size() const override { return ready() ? state_.size() : 0; }

private:
  IPersistence& persistence_;
  RecordKey key_;
  DeviceId stationId_;
  bool configured_;
  FixedRequestRepository<StorageCapacity> state_;
  uint8_t encoded_[kMaxEncodedBytes];
  uint16_t encodedLength_;

  bool ready() const { return configured_; }

  template <typename Operation>
  Status applyRecordTransition(Operation operation, RequestRecord& updated) {
    if (!ready())
      return Status(ErrorCode::INVALID_STATE);

    FixedRequestRepository<StorageCapacity> candidate = state_;
    RequestRecord result = {};
    const Status operationStatus = operation(candidate, result);

    if (!sameState(state_, candidate)) {
      if (!persistSnapshot(candidate))
        return Status(ErrorCode::STORAGE_FAILURE);
      state_ = candidate;
    }

    if (operationStatus.ok() || result.id.originStationId != 0)
      updated = result;
    return operationStatus;
  }

  static bool sameRecord(const RequestRecord& a, const RequestRecord& b) {
    return RequestIdentity::equals(a.id, b.id) && a.destinationId == b.destinationId &&
           a.createdAtMs == b.createdAtMs && a.expiresAtMs == b.expiresAtMs &&
           a.state == b.state && a.assignedCartId == b.assignedCartId &&
           a.assignmentEpoch == b.assignmentEpoch &&
           a.lastJourneyEventSequence == b.lastJourneyEventSequence &&
           a.claimState == b.claimState && a.claimCartId == b.claimCartId &&
           a.claimTransactionId == b.claimTransactionId;
  }

  static bool sameState(const FixedRequestRepository<StorageCapacity>& a,
                        const FixedRequestRepository<StorageCapacity>& b) {
    if (a.size() != b.size() || a.nextSequence() != b.nextSequence() ||
        a.nextGeneration() != b.nextGeneration() ||
        a.identifiersExhausted() != b.identifiersExhausted())
      return false;
    for (uint16_t i = 0; i < a.size(); ++i) {
      RequestRecord lhs = {};
      RequestRecord rhs = {};
      if (!a.recordAt(i, lhs) || !b.recordAt(i, rhs) || !sameRecord(lhs, rhs))
        return false;
    }
    return true;
  }

  static void put8(uint8_t* out, uint16_t& offset, uint8_t value) {
    out[offset++] = value;
  }

  static void put16(uint8_t* out, uint16_t& offset, uint16_t value) {
    put8(out, offset, static_cast<uint8_t>(value >> 8));
    put8(out, offset, static_cast<uint8_t>(value));
  }

  static void put32(uint8_t* out, uint16_t& offset, uint32_t value) {
    put8(out, offset, static_cast<uint8_t>(value >> 24));
    put8(out, offset, static_cast<uint8_t>(value >> 16));
    put8(out, offset, static_cast<uint8_t>(value >> 8));
    put8(out, offset, static_cast<uint8_t>(value));
  }

  static uint8_t get8(const uint8_t* in, uint16_t& offset) { return in[offset++]; }

  static uint16_t get16(const uint8_t* in, uint16_t& offset) {
    const uint16_t high = get8(in, offset);
    return static_cast<uint16_t>((high << 8) | get8(in, offset));
  }

  static uint32_t get32(const uint8_t* in, uint16_t& offset) {
    const uint32_t b0 = get8(in, offset);
    const uint32_t b1 = get8(in, offset);
    const uint32_t b2 = get8(in, offset);
    const uint32_t b3 = get8(in, offset);
    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
  }

  static uint32_t crc32(const uint8_t* data, uint16_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint16_t i = 0; i < length; ++i) {
      crc ^= data[i];
      for (uint8_t bit = 0; bit < 8; ++bit)
        crc = (crc & 1UL) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFUL;
  }

  static void
  encodeRecord(const RequestRecord& record, uint8_t* out, uint16_t& offset) {
    put16(out, offset, record.id.originStationId);
    put32(out, offset, record.id.sequence);
    put16(out, offset, record.id.generation);
    put16(out, offset, record.destinationId);
    put32(out, offset, record.createdAtMs);
    put32(out, offset, record.expiresAtMs);
    put8(out, offset, static_cast<uint8_t>(record.state));
    put16(out, offset, record.assignedCartId);
    put16(out, offset, record.assignmentEpoch);
    put32(out, offset, record.lastJourneyEventSequence);
    put8(out, offset, static_cast<uint8_t>(record.claimState));
    put16(out, offset, record.claimCartId);
    put32(out, offset, record.claimTransactionId);
  }

  static RequestRecord decodeRecord(const uint8_t* in, uint16_t& offset) {
    RequestRecord record = {};
    record.id.originStationId = get16(in, offset);
    record.id.sequence = get32(in, offset);
    record.id.generation = get16(in, offset);
    record.destinationId = get16(in, offset);
    record.createdAtMs = get32(in, offset);
    record.expiresAtMs = get32(in, offset);
    record.state = static_cast<RequestState>(get8(in, offset));
    record.assignedCartId = get16(in, offset);
    record.assignmentEpoch = get16(in, offset);
    record.lastJourneyEventSequence = get32(in, offset);
    record.claimState = static_cast<ClaimState>(get8(in, offset));
    record.claimCartId = get16(in, offset);
    record.claimTransactionId = get32(in, offset);
    return record;
  }

  bool persistSnapshot(const FixedRequestRepository<StorageCapacity>& source) {
    uint16_t offset = 0;
    put32(encoded_, offset, 0x4341524AUL); // "CARJ"
    put16(encoded_, offset, 1);            // schema version
    put16(encoded_, offset, source.size());
    put16(encoded_, offset, stationId_);
    put32(encoded_, offset, source.nextSequence());
    put16(encoded_, offset, source.nextGeneration());
    put8(encoded_, offset, source.identifiersExhausted() ? 1 : 0);

    for (uint16_t i = 0; i < source.size(); ++i) {
      RequestRecord record = {};
      if (!source.recordAt(i, record))
        return false;
      encodeRecord(record, encoded_, offset);
    }

    const uint32_t checksum = crc32(encoded_, offset);
    put32(encoded_, offset, checksum);
    encodedLength_ = offset;
    return persistence_.storeAtomically(key_, encoded_, encodedLength_).ok();
  }

  bool decodeSnapshot(uint16_t length) {
    if (length < kHeaderBytes + kChecksumBytes)
      return false;

    const uint32_t expectedChecksum =
        (static_cast<uint32_t>(encoded_[length - 4]) << 24) |
        (static_cast<uint32_t>(encoded_[length - 3]) << 16) |
        (static_cast<uint32_t>(encoded_[length - 2]) << 8) |
        static_cast<uint32_t>(encoded_[length - 1]);
    if (crc32(encoded_, static_cast<uint16_t>(length - 4)) != expectedChecksum)
      return false;

    uint16_t offset = 0;
    if (get32(encoded_, offset) != 0x4341524AUL || get16(encoded_, offset) != 1)
      return false;

    const uint16_t count = get16(encoded_, offset);
    const DeviceId storedStationId = get16(encoded_, offset);
    const uint32_t nextSequence = get32(encoded_, offset);
    const uint16_t nextGeneration = get16(encoded_, offset);
    const uint8_t exhausted = get8(encoded_, offset);

    if (storedStationId != stationId_ || count > StorageCapacity || exhausted > 1 ||
        static_cast<uint32_t>(length) !=
            static_cast<uint32_t>(kHeaderBytes) +
                (static_cast<uint32_t>(count) * 34UL) + kChecksumBytes)
      return false;

    for (uint16_t i = 0; i < count; ++i) {
      const RequestRecord record = decodeRecord(encoded_, offset);
      if (record.id.originStationId != stationId_ || !state_.restoreRecord(record).ok())
        return false;
    }
    if (offset != length - kChecksumBytes)
      return false;

    return state_.restoreAllocatorState(nextSequence, nextGeneration, exhausted != 0)
        .ok();
  }
};

} // namespace ca
