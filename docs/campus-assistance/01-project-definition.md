# 01 — Project Definition

## 1. Project title

**LoRa-Based Campus Mobility Assistance System for Blind and Visually Impaired Students**

Working short name: **Campus Assistance**.

## 2. Background

Large educational campuses often contain separated facilities: academic departments, libraries, laboratories, administrative buildings, canteens, mess facilities and other student services. A blind or visually impaired student may need assistance coordinating transport between these locations. A fixed-point request system can provide a simple way to request an electric cart without requiring the student to operate a smartphone application or depend on internet connectivity.

The proposed system uses fixed assistance stations and electric carts equipped with LoRa radio nodes. A station provides a tactile-button-driven and audible/visual interface. Carts receive requests over the radio network, queue them and present them to the driver. The driver manually reports progress using a small number of controls.

The system is an assistance-request and dispatch communication system. It is not an autonomous navigation, obstacle detection, vehicle control or emergency-response system.

## 3. Problem statement

Design and evaluate a low-cost, campus-deployable, internet-independent communication system that enables a blind or visually impaired student to request electric-cart assistance between registered campus locations. The system should provide understandable feedback at the request station, distribute requests to carts using LoRa, maintain a simple FIFO request queue, and let the driver report meaningful journey states with no more than two physical buttons per device.

The design must remain modular as the number of registered stations and carts grows, while avoiding expensive or complex peripherals and unnecessary controls.

## 4. Aim

To develop a modular LoRa-based campus assistance prototype that supports accessible station-based ride requests, configurable campus locations, cart-side FIFO request handling and manual journey-status reporting.

## 5. Objectives

1. Define a simple system architecture for fixed stations and electric carts.
2. Implement raw-LoRa communication between station and cart nodes without requiring internet infrastructure.
3. Provide station-side selection of a registered destination using a minimal physical interface.
4. Provide audible feedback using a buzzer and complementary text feedback on a 16×2 LCD.
5. Limit each station and each cart to a maximum of two physical buttons.
6. Support a scalable number of stations and carts through unique firmware-provisioned IDs.
7. Support any registered station as a destination, subject to the station registry configured in the devices.
8. Automatically receive and queue requests at carts, using FIFO order within each cart's queue.
9. Provide clear request lifecycle states: received, on the way, arrived at pickup and journey completed.
10. Allow a student to cancel an active request.
11. Provide duplicate detection, bounded queues, finite retry policies and explicit communication-failure feedback.
12. Produce reproducible hardware, firmware, protocol and validation documentation suitable for a final-year project report, research paper and potential patentability assessment.

## 6. Intended users and stakeholders

- **Primary user:** blind or visually impaired student requesting assistance.
- **Driver:** operates the electric cart and updates journey status.
- **Station maintainer:** installs and configures fixed station nodes.
- **Project administrator:** provisions device IDs and station registry data in firmware.
- **Institution:** provides campus locations, operational procedures, supervision and safety review.
- **Accessibility stakeholders:** students, disability support staff and accessibility specialists who evaluate the interaction design.

## 7. Scope

### Included

- Fixed pickup/request stations at campus locations.
- Electric carts equipped with LoRa nodes.
- Student pickup-station and destination selection.
- Firmware-configured station and cart IDs.
- Firmware-configured list of registered station IDs and labels.
- LoRa-only request, status, cancellation and acknowledgement communication.
- Per-cart FIFO request queue.
- Driver status control using at most two buttons.
- 16×2 LCD and buzzer feedback.
- Arduino Uno and ESP32 firmware targets.
- Bench testing, RF measurements, usability testing and supervised pilot evaluation.

### Excluded from the agreed initial scope

- GPS-based position tracking.
- Automatic cart approach detection or proximity sensing.
- Autonomous driving, steering, braking or motor control.
- Smartphone application or cloud backend.
- Wi-Fi, cellular, Bluetooth or internet-based communication.
- Spoken voice announcements; the selected feedback is simple buzzer patterns.
- Interface-based device registration. Initial registration is through firmware configuration.
- Automated route optimization or nearest-cart selection.
- Complex priority scheduling. The agreed queue discipline is FIFO.

## 8. Design principles

- Keep hardware inexpensive and readily replaceable.
- Use two physical buttons maximum per station and cart.
- Prefer state-dependent button actions over adding controls.
- Keep station and cart firmware role-specific and understandable.
- Keep queues and packet buffers bounded, especially on Arduino Uno.
- Make every status claim correspond to an actual received protocol event.
- Distinguish radio packet reception from human awareness and physical service completion.
- Do not represent prototype assumptions as experimentally proven results.

## 9. Expected academic outputs

- Working station and cart prototypes.
- Dedicated Arduino Uno and ESP32 firmware targets.
- Documented packet protocol and state machines.
- Hardware schematics, pin maps and bill of materials.
- RF link measurements and queue/reliability experiments.
- Accessibility evaluation with appropriate consent and supervision.
- Final-year project report and research paper.
- Prior-art review and a separately assessed patent disclosure, if a genuinely novel and non-obvious technical contribution is identified.

A patent is not guaranteed by building the system. Patentability depends on novelty, inventive step/non-obviousness, industrial applicability and jurisdiction-specific exclusions. A prior-art search and institutional IP review are required before public disclosure if patent filing is contemplated.
