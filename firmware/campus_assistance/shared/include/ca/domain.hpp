#pragma once
#include "types.hpp"
namespace ca {
class RequestIdentity{public:static bool isValid(const RequestId&);static bool equals(const RequestId&,const RequestId&);};
class PendingRequestStore{public:Status configure(uint16_t);Status insertIfAbsent(const RequestSummary&);bool peekOldest(RequestSummary&)const;bool remove(const RequestId&);uint16_t removeAssigned(const RequestId&);void clearLocal();bool contains(const RequestId&)const;uint16_t size()const;uint16_t capacity()const;};
class ActiveAssignment{public:bool isOccupied()const;Status activate(const AssignmentRecord&);Status updateJourneyStatus(JourneyStatus);Status complete();Status clearAfterTerminal();bool get(AssignmentRecord&)const;};
class DriverAvailabilityController{public:DriverMode mode()const;Status enableAcceptance();Status disableAcceptance();Status onAssignmentConfirmed(const AssignmentRecord&);Status onJourneyCompleted();bool mayClaim()const;};
class DestinationCatalog{public:Status load();uint16_t count()const;bool getByIndex(uint16_t,DeviceId&)const;bool isEnabled(DeviceId)const;};
class RequestLedger{public:Status add(const RequestId&);bool contains(const RequestId&)const;Status removeTerminalBefore(uint32_t);};
class RequestLifecycle{public:Status create(const NewRequest&,RequestId&);Status validateClaim(const ClaimRequest&,uint32_t)const;Status commitAssignment(const ClaimRequest&,AssignmentRecord&);Status cancel(const RequestId&);Status expireDue(uint32_t);Status applyJourneyUpdate(const RequestId&,DeviceId,JourneyStatus);};
class StationClaimCoordinator{public:Status evaluate(const ClaimRequest&,uint32_t)const;Status commitWinner(const ClaimRequest&,AssignmentRecord&);bool isAssigned(const RequestId&)const;};
class CartClaimClient{public:Status requestClaim(const RequestId&,DeviceId);void onClaimResult(StatusCode);void onAssignmentNotice(const AssignmentNotice&);bool hasClaimInFlight()const;};
class JourneyProgressController{public:Status advance(JourneyStatus);JourneyStatus current()const;};
class EventReconciler{public:Status applyAssignmentNotice(const AssignmentNotice&);Status applyCancellationNotice(const RequestId&);Status applyExpiryNotice(const RequestId&);};
}
