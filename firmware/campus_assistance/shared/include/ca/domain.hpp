#pragma once

// Aggregate domain contract header. Internal modules should include the
// specific contract they depend on from ca/domain/.
#include "ca/domain/active_assignment.hpp"
#include "ca/domain/cart_claim_client.hpp"
#include "ca/domain/destination_catalog.hpp"
#include "ca/domain/driver_availability_controller.hpp"
#include "ca/domain/event_reconciler.hpp"
#include "ca/domain/journey_progress_controller.hpp"
#include "ca/domain/pending_request_store.hpp"
#include "ca/domain/request_identity.hpp"
#include "ca/domain/request_lifecycle.hpp"
#include "ca/domain/station_claim_coordinator.hpp"
