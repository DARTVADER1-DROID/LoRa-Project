# 02 — System Requirements

## 1. Requirement status and priority

These requirements describe the agreed target system. They are not proof that the current source firmware implements them.

Priority: **M** = mandatory for the initial prototype; **S** = secondary/future enhancement.

## 2. Functional requirements

| ID | Requirement | Priority | Verification |
|---|---|---:|---|
| FR-01 | A student can initiate an assistance request at a registered station. | M | Functional test |
| FR-02 | A student can select a valid registered destination. | M | Destination-selection/accessibility test |
| FR-03 | Stations and carts communicate over LoRa only. | M | Architecture/source inspection |
| FR-04 | Every cart continuously listens for valid requests, regardless of driver acceptance mode. | M | Receive-state test |
| FR-05 | Each cart maintains a bounded FIFO list of live, unaccepted/unassigned requests. | M | Queue test |
| FR-06 | A cart may claim a request only when driver acceptance is enabled and it has no active assignment. | M | State-machine test |
| FR-07 | A cart may hold at most one accepted/active assignment at a time. | M | Multi-request test |
| FR-08 | The originating station commits at most one winning cart per request using a claim/assignment handshake. | M | Multi-cart contention test |
| FR-09 | When a request is assigned to one cart, other carts remove it from their pending lists after receiving the assignment announcement. | M | Multi-cart synchronization test |
| FR-10 | Disabling acceptance clears that cart's current local pending list but does not cancel station-owned requests. | M | Driver-mode test |
| FR-11 | Requests received after the clear may populate a new pending list while the cart remains not accepting. | M | Receive/queue test |
| FR-12 | After journey completion, the cart remains not accepting until the driver manually re-enables it. | M | Completion workflow test |
| FR-13 | The default lifetime of an unaccepted request is 10 minutes, configurable in firmware. | M | Expiry test |
| FR-14 | The originating station is authoritative for expiry and rejects late claims. | M | Time-boundary/claim test |
| FR-15 | Driver reports Received/Assigned, On the way, Arrived at pickup and Journey completed. | M | State-transition test |
| FR-16 | Student cancellation is supported and correlated to the full request identity. | M | Cancellation/race test |
| FR-17 | Station/cart IDs and station registry are firmware-provisioned and unique. | M | Configuration audit |
| FR-18 | Station count and cart count can be expanded within measured memory, airtime and RF limits. | M | Multi-node/load test |
| FR-19 | Every station and cart has no more than two physical buttons. | M | Hardware inspection |
| FR-20 | Station and cart provide 16×2 LCD text feedback and buzzer feedback. | M | Hardware/accessibility test |
| FR-21 | Duplicate, expired, cancelled and already-assigned requests cannot create duplicate assignments. | M | Fault-injection test |
| FR-22 | Queue-full, invalid request, no acknowledgement and communication uncertainty are reported explicitly. | M | Boundary/fault test |
| FR-23 | The system does not control vehicle motion or safety-critical vehicle functions. | M | Wiring/source review |

## 3. Non-functional requirements

| ID | Requirement | Target / note |
|---|---|---|
| NFR-01 | Low hardware cost | Avoid unnecessary peripherals and extra buttons; publish measured BOM costs. |
| NFR-02 | Simple operation | Two-button maximum, context-sensitive interface, LCD, buzzer and FIFO pending lists. |
| NFR-03 | Modularity | Variable station/cart count through firmware provisioning. |
| NFR-04 | MCU portability | Dedicated Uno and ESP32 builds using a common over-the-air protocol. |
| NFR-05 | Bounded memory | Static/bounded request lists and buffers; profile Uno SRAM. |
| NFR-06 | Resilience | Handle duplicates, collisions, lost ACKs, expiry, queue overflow and reboot. |
| NFR-07 | Accessibility | Nonvisual station selection must be validated with intended users; LCD alone is insufficient. |
| NFR-08 | Maintainability | Document IDs, protocol version, RF profile and commissioning procedure. |
| NFR-09 | Privacy | Do not broadcast student names or sensitive personal data. |
| NFR-10 | Safety | No vehicle actuation; driver remains responsible for safe operation. |

## 4. Interface requirements

### Student station

- Maximum two physical buttons, 16×2 LCD and buzzer.
- Context-sensitive button actions.
- Destination selection, request submission, progress feedback and cancellation.
- Nonvisual destination identification must not rely on LCD text alone.
- Exact audible/tactile selection method requires user testing.

### Driver cart console

- Maximum two physical buttons, 16×2 LCD and buzzer.
- Persistent driver-controlled Accepting Requests mode.
- Driver can enable and disable acceptance.
- Disabling acceptance clears the cart's current local pending list.
- Cart continues listening while not accepting; requests received after the clear may populate a fresh pending list.
- Cart claims only when accepting and idle; one active assignment maximum.
- Completion returns the cart to not-accepting; driver must manually re-enable.
- LCD shows mode, active request, pickup, destination and next permitted action.
- Controls should be used only when safely stopped.

## 5. Quantities to measure before setting performance targets

- RF range and packet delivery probability.
- Request-to-assignment latency.
- Status-update latency.
- Claim collision rate and contention recovery time.
- Maximum sustainable request arrival rate.
- Pending-list capacity and RAM use on each MCU.
- Power consumption and battery life.
- Audible feedback detectability and interaction success in representative conditions.

## 6. Supervised prototype acceptance criteria

1. Uno and ESP32 builds compile reproducibly and interoperate.
2. Multiple stations and carts can be uniquely provisioned.
3. All carts receive a request while acceptance is disabled, without claiming it.
4. Enabling an idle cart causes it to attempt the oldest eligible pending request.
5. A cart cannot accept a second request while serving one.
6. A winning assignment is confirmed by the station; other carts remove that request from pending.
7. Disabling acceptance clears only the local pending list.
8. Completion does not automatically re-enable acceptance.
9. Unaccepted requests expire at 10 minutes by station authority.
10. Cancellation, duplicates, queue-full, simultaneous claims, lost ACK and reboot cases produce explicit safe outcomes.
11. Two-button limit, LCD and buzzer are verified physically.
12. RF/electrical compliance and supervised operating procedures are reviewed before field trials.
