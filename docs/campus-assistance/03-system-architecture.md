# 03 — System Architecture

## 1. Architecture summary

Campus Assistance is a distributed raw-LoRa system made from fixed station nodes and mobile electric-cart nodes. It has no central server or internet dependency. All nodes use one common LoRa modem/network configuration and a compatible over-the-air protocol, regardless of whether the MCU is Arduino Uno or ESP32.

The station creates and owns each student request. Every cart continuously listens and maintains a bounded local list of **unaccepted, unassigned requests**. A driver controls whether their cart is willing to accept a request. A cart may hold only one accepted/active assignment at a time. Pending requests are not a cart's service queue: they are a local view of requests that remain available for any eligible cart to claim.

The design uses a claim/assignment handshake to ensure that only one cart becomes responsible for a request. The originating station is authoritative for request identity, cancellation and the default 10-minute expiry.

## 2. Logical components

### 2.1 Station node

- Firmware-configured unique station ID and location label.
- Configured registry of valid campus destinations.
- Accessible two-button, 16×2 LCD and buzzer interface.
- Creates a unique request identity and broadcasts the request.
- Owns the request's creation time and expiry deadline.
- Receives claims and commits at most one cart assignment for a request.
- Broadcasts assignment, cancellation and expiry outcomes.
- Displays/announces only states supported by received protocol events.
- Rejects invalid destinations, stale requests and conflicting late events.

### 2.2 Cart node

- Firmware-configured unique cart ID.
- Continuously listens for valid campus requests, including when not accepting.
- Validates and deduplicates requests.
- Maintains a bounded FIFO pending list containing only unaccepted/unassigned requests.
- Maintains at most one accepted/active assignment.
- Exposes a persistent driver-controlled **Accepting Requests** mode.
- Claims a pending request only when the driver has enabled acceptance and the cart has no active assignment.
- Removes a request from its pending list when it learns that any cart has been assigned it.
- Clears its current local pending list when the driver disables acceptance; it does not cancel the station-owned requests.
- Sends driver-entered journey status updates and waits for matching acknowledgements.

### 2.3 Human roles

- Student: selects a destination and submits/cancels a request at a station.
- Driver: controls request acceptance and manually reports journey progress.
- Administrator: provisions IDs, station registry, network settings and firmware during commissioning.

## 3. Network topology

All stations and carts share one LoRa broadcast domain. A request may be heard by multiple carts. This is expected: receiving a request is not the same as accepting or owning it.

The system must not imply unlimited RF coverage. Buildings, walls, vehicle bodies, antenna placement, interference, legal transmit settings and modem configuration determine actual coverage.

## 4. Device identity and configuration

Example identities:

| Device | Human-readable label | Example numeric ID |
|---|---|---:|
| Station | ST001 | 1 |
| Station | ST002 | 2 |
| Cart | CRT001 | 1001 |
| Cart | CRT002 | 1002 |

Exact ID width and allocation remain protocol-design items. IDs must be unique within the campus network and must not be reused in a way that makes old packets appear current.

Firmware-level provisioning is the initial approach:
- No registration menu, PIN entry or over-the-air enrollment.
- Administrator edits configuration, compiles and flashes the node.
- Commissioning records the physical device, role, ID, location and firmware/protocol version.
- Adding a station or cart means adding a valid configuration entry and commissioning the new node.

## 5. Request ownership and local pending queues

### 5.1 Request identity and ownership

A request identity must include at least origin station ID, a station-local sequence and a generation/session discriminator. The origin station is authoritative for whether the request is still live and whether a cart assignment has been committed.

### 5.2 Cart pending-list rules

1. Every cart listens continuously.
2. A valid, live, unassigned request may be inserted once into that cart's bounded FIFO pending list, regardless of driver mode.
3. A pending entry is not an accepted assignment.
4. A cart may attempt a claim only if its driver has enabled acceptance and it has no active assignment.
5. On successful assignment confirmation, the winning cart removes the request from its pending list and stores it as its sole active assignment.
6. Other carts remove the same request from their pending lists when they receive the station's assignment announcement.
7. A cart must never claim a second request while it has an active assignment.
8. When the driver disables acceptance, the cart clears its local pending list. This is not a student cancellation and does not cancel the station-owned request.
9. Requests received after that clear may be added to a new local pending list even while acceptance remains disabled.
10. Expired, cancelled, malformed, duplicate or already-assigned requests are not eligible for a claim.

A cart's local pending list is therefore a cache/view of still-unassigned requests, not a globally authoritative queue. It can differ temporarily between carts because packets can be lost. Assignment and expiry announcements, periodic refresh/rebroadcast and reconciliation rules must converge those views.

### 5.3 FIFO meaning

FIFO applies to the order of unique, valid, unassigned requests observed and retained by an individual cart. There is no global campus-wide FIFO guarantee because carts do not share a central scheduler or globally synchronized ordering source.

## 6. Driver-gated claim and assignment algorithm

Target algorithm:

1. Cart receives, validates and deduplicates a request.
2. If it is still live and unassigned, cart adds it to its local pending FIFO list.
3. If acceptance is disabled or the cart already has an active assignment, it does not claim.
4. When acceptance is enabled and the cart is idle, it examines the oldest pending request.
5. It checks that the request has not expired, been cancelled or already assigned.
6. It sends a claim containing the full request identity and cart ID.
7. The originating station validates the claim and atomically commits at most one winning cart.
8. The station broadcasts an assignment confirmation identifying the winner.
9. The winning cart accepts the assignment only after receiving the matching confirmation, removes the request from pending and records it as active.
10. Other carts remove the request from pending when they receive the assignment confirmation.
11. The cart stops claiming and waits for the active journey to reach a terminal state.
12. After completion, the driver must manually re-enable acceptance. Acceptance is not automatically restored.

If simultaneous claims collide or the claim/assignment acknowledgement is lost, no cart may assume it won. The request remains unconfirmed until the station's committed assignment can be established. Collision mitigation, retries, timeouts and recovery are specified as protocol work and must be tested.

## 7. Request expiry and cancellation authority

- Default lifetime for an unaccepted request: **10 minutes from station creation**.
- The timeout is a firmware-configurable parameter, not a hardcoded protocol constant.
- The originating station is authoritative for the deadline and emits an expiry outcome.
- Carts independently age local entries as a defensive cleanup, but must not use local clock disagreement to override a station-confirmed assignment or expiry.
- A late claim for an expired request must be rejected.
- Student cancellation is a station-originated terminal event correlated to the full request identity.
- An assignment/cancellation race must have deterministic station-side resolution; terminal events must not be silently overwritten.

## 8. Scaling and MCU variants

“Scalable” means the architecture and identifiers are not hardcoded to one station/cart. It does not mean unlimited devices. Practical limits include ID width, registry storage, Uno SRAM/flash, queue capacity, airtime, regulatory duty-cycle, collisions, packet rate and RF coverage.

Arduino Uno and ESP32 require dedicated hardware-specific firmware builds but share:
- Common packet schema and protocol version.
- Common RF/modem configuration.
- Common request and assignment state semantics.
- Common test vectors for cross-MCU interoperability.

Pin maps, storage backends, memory limits and task scheduling remain board-specific. The exact ESP32 board is not yet selected.

## 9. Failure boundaries

- Reception ACK means a valid frame was processed by an MCU; it does not prove a driver saw it.
- Assignment is confirmed only by the station's matching assignment confirmation.
- Status ACK does not prove physical arrival or safe vehicle operation.
- If assignment ownership is ambiguous, do not silently assign a second cart.
- If expiry/cancellation cannot be confirmed, display a communication-uncertain state rather than claiming success.
- No electronics in this system control steering, braking, throttle or traction.

## 10. Architecture status

This document defines the agreed target architecture and an engineering algorithm for implementation. It does not claim that the current campus firmware already implements driver-gated acceptance, cross-cart pending-list cleanup, 10-minute expiry or the final claim protocol. Those are implementation and verification tasks.
