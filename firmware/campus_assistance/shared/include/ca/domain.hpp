#pragma once

#include "ports.hpp"

namespace ca {

class RequestIdentity {
public:
  static bool isValid(const RequestId& id);
  static bool equals(const RequestId& lhs, const RequestId& rhs);
  static uint32_t hash(const RequestId& id);
};

class PendingRequestStore {
public:
  virtual ~PendingRequestStore() {}
  virtual Status configure(uint16_t capacity) = 0;
  virtual Status insertIfAbsent(const RequestSummary& request) = 0;
  virtual bool peekOldest(RequestSummary& request) const = 0;
  virtual bool remove(const RequestId& id) = 0;
  virtual uint16_t removeExpired(const ExpiredRequestSet& expired) = 0;
  virtual uint16_t removeAssigned(const RequestId& id) = 0;
  virtual void clearLocal() = 0;
  virtual bool contains(const RequestId& id) const = 0;
  virtual uint16_t size() const = 0;
  virtual uint16_t capacity() const = 0;
};

class ActiveAssignment {
public:
  virtual ~ActiveAssignment() {}
  virtual bool isOccupied() const = 0;
  virtual Status activate(const AssignmentRecord& stationConfirmed) = 0;
  virtual Status updateJourneyStatus(JourneyStatus status, EventSequence sequence) = 0;
  virtual Status complete(const DriverAction& action) = 0;
  virtual Status clearAfterTerminal() = 0;
  virtual bool get(AssignmentRecord& assignment) const = 0;
};

class DriverAvailabilityController {
public:
  virtual ~DriverAvailabilityController() {}
  virtual AcceptanceMode acceptanceMode() const = 0;
  virtual CartServiceState serviceState() const = 0;
  virtual Status enableAcceptance(const DriverAction& action) = 0;
  virtual Status disableAcceptance(const DriverAction& action) = 0;
  virtual Status onAssignmentConfirmed(const AssignmentRecord& assignment) = 0;
  virtual Status onJourneyCompleted() = 0;
  virtual bool mayClaim() const = 0;
};

class DestinationCatalog {
public:
  virtual ~DestinationCatalog() {}
  virtual Status load(const DestinationConfig& config) = 0;
  virtual uint16_t count() const = 0;
  virtual bool getByIndex(uint16_t index, Destination& destination) const = 0;
  virtual bool findById(DeviceId id, Destination& destination) const = 0;
  virtual bool isEnabled(DeviceId id) const = 0;
};

class RequestLifecycle {
public:
  virtual ~RequestLifecycle() {}
  virtual Status create(const NewRequest& input, RequestRecord& created) = 0;
  virtual Status cancel(const RequestId& id, CancellationSource source) = 0;
  virtual Status expireDue(uint32_t stationNowMs, ExpiredRequestSet& expired) = 0;
  virtual Status applyJourneyUpdate(const JourneyUpdate& update) = 0;
  virtual bool find(const RequestId& id, RequestRecord& record) const = 0;
};

// Sole station-side claim transaction and assignment-commit authority.
class StationClaimCoordinator {
public:
  virtual ~StationClaimCoordinator() {}
  virtual Status evaluate(const ClaimRequest& claim, uint32_t stationNowMs) const = 0;
  virtual Status withdrawClaim(const ClaimWithdraw& withdrawal) = 0;
  virtual Status commitWinner(const ClaimRequest& claim,
                              AssignmentRecord& assignment) = 0;
  virtual bool isAssigned(const RequestId& id) const = 0;
};

// Cart-side claim transaction only. It never activates an assignment.
class CartClaimClient {
public:
  virtual ~CartClaimClient() {}
  virtual Status requestClaim(const RequestId& id, DeviceId cartId) = 0;
  virtual Status withdrawClaim(const ClaimWithdraw& withdrawal) = 0;
  virtual Status onClaimResult(const ClaimResult& result) = 0;
  virtual bool hasClaimInFlight() const = 0;
  virtual bool getOutstandingClaim(ClaimRequest& claim) const = 0;
};

// Stateless transition policy. ActiveAssignment remains the sole owner of
// current assignment/journey state.
class JourneyProgressController {
public:
  virtual ~JourneyProgressController() {}
  virtual Status validateTransition(JourneyStatus current,
                                    JourneyStatus next,
                                    const DriverAction& action) const = 0;
};

// Orchestrates authoritative events into the domain owners; stores no
// duplicate assignment or journey state of its own.
class EventReconciler {
public:
  virtual ~EventReconciler() {}
  virtual Status applyAssignmentNotice(const AssignmentNotice& notice) = 0;
  virtual Status applyCancellationNotice(const CancellationNotice& notice) = 0;
  virtual Status applyExpiryNotice(const ExpiryNotice& notice) = 0;
  virtual Status applySnapshot(const RequestSnapshot& snapshot) = 0;
};

} // namespace ca
