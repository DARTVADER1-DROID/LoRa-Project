#pragma once

#include "ca/domain/event_reconciler.hpp"
#include "ca/domain/active_assignment.hpp"
#include "ca/domain/driver_availability_controller.hpp"
#include "ca/domain/pending_request_store.hpp"
#include "ca/domain/cart_claim_client.hpp"

namespace ca {

// Stateless orchestration of station-authoritative events for one cart.
class CartEventReconciler final : public EventReconciler {
public:
  CartEventReconciler(DeviceId cartId,
                      PendingRequestStore& pendingRequests,
                      ActiveAssignment& activeAssignment,
                      DriverAvailabilityController& availability,
                      CartClaimClient& claimClient);

  Status applyAssignmentNotice(const AssignmentNotice& notice) override;
  Status applyCancellationNotice(const CancellationNotice& notice) override;
  Status applyExpiryNotice(const ExpiryNotice& notice) override;
  Status applySnapshot(const RequestSnapshot& snapshot) override;

private:
  DeviceId cartId_;
  PendingRequestStore& pendingRequests_;
  ActiveAssignment& activeAssignment_;
  DriverAvailabilityController& availability_;
  CartClaimClient& claimClient_;

  void resolveLostClaim(const RequestId& id, ErrorCode reason);
};

} // namespace ca
