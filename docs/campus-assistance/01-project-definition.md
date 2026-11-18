# 01 — Project Definition

## 1. Project title

**LoRa-Based Campus Mobility Assistance System for Blind and Visually Impaired Students**

Working short name: **Campus Assistance**.

## 2. Background

Large educational campuses contain separated facilities such as departments, libraries, laboratories, administrative buildings, canteens and mess facilities. A blind or visually impaired student may need assistance coordinating transport between these locations. A fixed-point request system can provide a simple way to request an electric cart without requiring a smartphone application or internet connectivity.

The proposed system uses fixed assistance stations and electric carts equipped with LoRa nodes. Students submit requests at stations. All carts listen continuously and maintain local lists of requests that remain unaccepted and unassigned. A driver explicitly enables the cart to accept a request. Each cart serves at most one accepted request at a time and manually reports journey progress.

The system is an assistance-request and dispatch communication system. It is not an autonomous navigation, obstacle detection, vehicle control or emergency-response system.

## 3. Problem statement

Design and evaluate a low-cost, campus-deployable, internet-independent communication system that enables a blind or visually impaired student to request electric-cart assistance between registered campus locations. The system should provide accessible station interaction, distribute requests over LoRa, maintain local FIFO lists of unaccepted requests, respect driver-controlled availability, prevent duplicate cart assignment and provide manual journey-status reporting using no more than two physical buttons per device.

## 4. Aim

To develop a modular LoRa-based campus assistance prototype supporting accessible station requests, driver-gated cart assignment, one active ride per cart, local FIFO pending-request handling, configurable campus locations and manual journey-status reporting.

## 5. Objectives

1. Define a distributed architecture for fixed stations and electric carts.
2. Implement raw-LoRa communication without internet infrastructure.
3. Provide station-side destination selection using a minimal accessible interface.
4. Provide buzzer feedback and complementary 16×2 LCD text.
5. Limit each station and cart to a maximum of two physical buttons.
6. Support scalable station/cart identities through firmware provisioning.
7. Support registered stations as destinations.
8. Make every cart continuously listen for requests and maintain a bounded local FIFO list of unaccepted/unassigned requests.
9. Require explicit driver enablement before a cart claims a request.
10. Limit each cart to one accepted/active assignment at a time.
11. Coordinate claims and assignment so only one cart wins a request.
12. Clear a cart's local pending list when its driver disables acceptance, without cancelling the student-owned request.
13. Require manual driver reactivation after journey completion.
14. Expire unaccepted requests after a configurable duration, initially 10 minutes.
15. Provide Received/Assigned, On the way, Arrived at pickup and Journey completed lifecycle feedback.
16. Support student cancellation, duplicate detection, bounded retries and explicit communication-failure feedback.
17. Produce reproducible hardware, firmware, protocol and validation documentation for a final-year project, research paper and patentability assessment.

## 6. Intended users and stakeholders

- Primary user: blind or visually impaired student.
- Driver: operates the cart, controls request acceptance and reports journey progress.
- Station maintainer: installs and configures fixed station nodes.
- Project administrator: provisions IDs, station registry and firmware.
- Institution: provides campus locations, operational procedures, supervision and safety review.
- Accessibility stakeholders: evaluate the interface with appropriate consent.

## 7. Scope

### Included

- Fixed campus request stations and electric carts.
- Student pickup-station and destination selection.
- Firmware-configured station/cart IDs and station registry.
- LoRa-only request, claim, assignment, status, cancellation, expiry and acknowledgement messages.
- Per-cart FIFO pending list of unaccepted/unassigned requests.
- Zero-or-one active assignment per cart.
- Persistent driver-controlled Accepting Requests mode.
- Continuous cart listening, including while not accepting.
- Local pending-list clear when acceptance is disabled.
- Manual reactivation after journey completion.
- Default 10-minute configurable expiry for unaccepted requests.
- Two-button maximum, 16×2 LCD and buzzer.
- Arduino Uno and ESP32 dedicated firmware targets.
- Bench, RF, usability and supervised pilot testing.

### Excluded from the initial scope

- GPS-based position tracking.
- Automatic cart approach detection.
- Autonomous driving, steering, braking or motor control.
- Smartphone application, cloud backend or internet dependency.
- Wi-Fi, cellular or Bluetooth communication.
- Spoken voice announcements.
- Interface-based device registration.
- Automated route optimization or nearest-cart selection.
- Priority scheduling beyond FIFO pending-list order.

## 8. Design principles

- Keep hardware low-cost and avoid unnecessary controls.
- Separate pending unaccepted requests from an accepted active assignment.
- Never treat packet reception or a transmitted claim as proof of assignment.
- Make the originating station authoritative for assignment and expiry.
- Keep queues, buffers, retries and transactions bounded.
- Make all user-visible status claims evidence-based.
- Distinguish MCU acknowledgement from driver awareness and physical service completion.
- Do not present assumptions as measured outcomes.

## 9. Expected academic outputs

- Working station and cart prototypes.
- Dedicated Arduino Uno and ESP32 firmware builds.
- Documented packet protocol, algorithms and state machines.
- Schematics, pin maps and bill of materials.
- RF, queue, latency, power and usability measurements.
- Final-year project report and research paper.
- Prior-art review and institutional IP assessment, if a patent disclosure is justified.

Patentability is not guaranteed. Novelty, inventive step/non-obviousness, industrial applicability and jurisdiction-specific rules must be assessed before public disclosure if filing is contemplated.
