# Campus Assistance System — Engineering Documentation

**Project:** LoRa-Based Campus Mobility Assistance for Blind and Visually Impaired Students  
**Academic purpose:** Final-year engineering project, research-paper development and patentability exploration  
**Document status:** Target requirements and engineering design. Not a claim that every feature is implemented or validated.

## 1. Start here

This folder is the project-level engineering specification, separate from the repository's messaging-firmware documentation. Read the documents in this order:

| Order | Document | What it establishes |
|---:|---|---|
| 1 | [Project definition](01-project-definition.md) | Problem, aim, scope, stakeholders and academic outputs |
| 2 | [System requirements](02-requirements.md) | Testable functional, non-functional and interface requirements |
| 3 | [System architecture](03-system-architecture.md) | Nodes, LoRa topology, driver-gated assignment, ownership and scaling |
| 4 | [Operating workflow](04-operating-workflow.md) | Student/driver workflows, two-button interfaces and state machines |
| 5 | [Hardware design](05-hardware-design.md) | Uno, ESP32, RA-02, LCD, buttons, buzzer, power and electrical constraints |
| 6 | [LoRa communication design](06-lora-communication-design.md) | RF constraints, message families, airtime, retries, security and regulation |
| 7 | [Firmware and data model](07-firmware-and-data-model.md) | Firmware modules, request identity, pending lists, active assignment and expiry |
| 8 | [Safety, privacy and accessibility](08-safety-accessibility.md) | Safety boundaries, nonvisual interaction, privacy and risks |
| 9 | [Verification and research plan](09-verification-research-plan.md) | Functional/RF/accessibility tests, metrics, research and patent preparation |
| 10 | [Decisions and open engineering items](10-decisions-and-open-items.md) | Confirmed decisions, proposals, unresolved choices and implementation sequence |

Supporting diagrams:
- [System architecture diagram](diagrams/system-architecture.md)
- [Request sequence diagram](diagrams/request-sequence.md)

## 2. Current agreed design at a glance

- Fixed stations are installed at campus locations; electric carts provide transport.
- Arduino Uno and ESP32 are target platforms with dedicated firmware builds and one common over-the-air protocol/LoRa configuration.
- Retain Ai-Thinker RA-02 / SX1278, subject to verification of applicable Indian WPC requirements before field transmission.
- Every cart continuously listens for requests.
- Each cart keeps a local FIFO list of unaccepted/unassigned requests.
- The driver explicitly enables persistent **Accepting Requests** mode.
- A cart claims only while acceptance is enabled and it has no active assignment.
- A cart holds at most one accepted/active assignment at a time.
- A station-authoritative claim/assignment handshake ensures only one cart wins a request; other carts remove it from pending when they learn of the assignment.
- Disabling acceptance clears that cart's current local pending list, but does not cancel the student request. The cart continues listening and can collect newly received requests into a fresh pending list.
- After journey completion, the driver must manually re-enable acceptance.
- Unaccepted requests expire after **10 minutes by default**, configurable in firmware; the originating station is authoritative.
- Each station and cart has at most two physical buttons, a 16×2 LCD and buzzer.
- Initial station/cart registration is firmware-level only.
- Student cancellation and manual driver progress updates are supported.

## 3. Important queue terminology

Do not call the cart's pending list a service queue:
- **Pending list:** requests heard by a cart that are still unaccepted and unassigned. FIFO order is local to that cart.
- **Active assignment:** the single request that this cart has accepted and is responsible for serving.

No cart may hold more than one active assignment. A request assigned to another cart must be removed from local pending lists when the assignment is received.

## 4. Reading conventions

- **Confirmed decision:** explicitly selected by the project owner.
- **Engineering proposal:** recommended implementation detail requiring specification, review or testing.
- **Open item:** decision or evidence still required.
- **Existing implementation:** behavior present in repository source at the time of review.

The target specification and current firmware must not be conflated. Refer to document 10 for known gaps.

## 5. Regulatory and safety gates

The RA-02/SX1278 is commonly sold as a 433 MHz module. A 433 MHz module cannot be software-tuned into the 865–867 MHz band. Confirm the exact module, antenna, frequency, EIRP, bandwidth, duty cycle and applicable Indian WPC requirements before transmitting in a campus environment.

The system coordinates assistance requests only. It is not a vehicle control, autonomous navigation or safety system. No connection to steering, braking, throttle or traction control is permitted in this project scope.

## 6. Revision discipline

When a decision changes:
1. Update this decision register.
2. Update requirements and affected architecture/workflow/data-model documents.
3. Update diagrams and test cases.
4. Mark whether the change is a target requirement, implementation proposal or measured result.
5. Do not claim firmware compliance until code and tests demonstrate it.

No unmeasured range, queue capacity, battery life, cost, delivery reliability or accessibility outcome is presented as a result.
