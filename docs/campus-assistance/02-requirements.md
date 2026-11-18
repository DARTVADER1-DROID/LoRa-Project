# 02 — System Requirements

## 1. Requirement status

The requirements below record the agreed target. They are not all implemented in the current source firmware. Verification status must be updated only after a test is performed and evidence is recorded.

Priority: **M** = mandatory for initial prototype; **S** = secondary/future enhancement.

## 2. Functional requirements

| ID | Requirement | Priority | Verification |
|---|---|---:|---|
| FR-01 | A student can initiate an assistance request at a registered station. | M | Functional test |
| FR-02 | The student can select any registered station as the destination, except an invalid/unregistered destination. | M | Destination-selection test |
| FR-03 | Station and cart nodes communicate using LoRa only. | M | Architecture/source inspection |
| FR-04 | A cart automatically receives and queues valid requests; no per-request driver acceptance is required to enqueue. | M | Multi-request test |
| FR-05 | Each cart processes its pending requests in FIFO order. | M | Queue-order test |
| FR-06 | The driver can update a request to Received, On the way, Arrived at pickup and Journey completed. | M | State-transition test |
| FR-07 | Student cancellation is supported before completion. | M | Cancellation test |
| FR-08 | Station and cart IDs are configured in firmware and unique within the campus network. | M | Configuration audit |
| FR-09 | Station registry can be expanded by adding firmware configuration entries, without changing the packet concept. | M | Multi-station configuration test |
| FR-10 | Cart registration is likewise firmware-configured and scalable. | M | Multi-cart configuration test |
| FR-11 | Every node has no more than two physical buttons. | M | Hardware inspection |
| FR-12 | The station and cart use a 16×2 character LCD for text feedback. | M | Hardware inspection |
| FR-13 | The station and cart use a buzzer for simple audible status patterns. | M | Functional/accessibility test |
| FR-14 | The user can distinguish request confirmation, progress, cancellation and error feedback without relying only on colour. | M | Accessibility test |
| FR-15 | Duplicate packets must not create duplicate service requests or duplicate queue entries. | M | Fault-injection test |
| FR-16 | The system must communicate queue-full or invalid-request conditions explicitly. | M | Boundary test |
| FR-17 | Each device must reject packets for an incompatible network identifier or malformed frame. | M | Protocol test |
| FR-18 | Retries, timeouts and queue capacity must be finite and documented. | M | Code review/test |
| FR-19 | The system must not control vehicle motion or safety-critical vehicle functions. | M | Wiring and source review |

## 3. Non-functional requirements

| ID | Requirement | Target / note |
|---|---|---|
| NFR-01 | Low hardware cost | Select common modules and avoid unnecessary peripherals; publish measured BOM costs. |
| NFR-02 | Simplicity | Two-button maximum, compact LCD, simple beep patterns, FIFO scheduling. |
| NFR-03 | Modularity | Station and cart roles can be deployed in variable counts. |
| NFR-04 | MCU portability | Dedicated firmware configurations for Uno and ESP32; do not assume identical pinouts or electrical levels. |
| NFR-05 | Bounded memory | Static, bounded queues and buffers; especially important for Uno's limited SRAM. |
| NFR-06 | Resilience | Detect duplicates, lost acknowledgements, timeouts, queue overflow and node restart. |
| NFR-07 | Accessibility | Audio feedback and low-complexity interaction; validate with intended users. |
| NFR-08 | Maintainability | Documented device IDs, firmware version, network configuration and commissioning checklist. |
| NFR-09 | Privacy | Transmit only operational request data; avoid student names or sensitive personal data in broadcast packets. |
| NFR-10 | Safety | No actuation of vehicle systems; driver remains responsible for safe operation. |

## 4. Interface requirements

### Station interface

- Two buttons maximum.
- 16×2 LCD.
- Buzzer.
- Button actions depend on the current screen/state.
- The interface must support destination selection, request confirmation and cancellation.
- A student must not need to interpret LED colour as the only feedback.
- Button labels, tactile differentiation and enclosure placement are part of the accessibility design and must be evaluated.

### Cart interface

- Two buttons maximum.
- 16×2 LCD.
- Buzzer.
- Requests enter the queue automatically.
- Driver uses the buttons to inspect/advance the active request and report journey status.
- The LCD must show enough information to distinguish the active request, pickup station, destination and current state.
- Button actions must be guarded by state so an accidental press cannot skip required journey stages.

## 5. Performance requirements still to be quantified

Do not invent target values before the first site survey and RF tests. The following must be measured and then assigned acceptance thresholds:

- Maximum station-to-cart distance and worst-case indoor/outdoor path.
- Packet delivery probability at each intended location.
- Request-to-cart-reception latency.
- Status update latency.
- Maximum sustainable request arrival rate.
- Queue capacity on each MCU and RAM usage at maximum configured nodes.
- Battery operating time, if battery powered.
- Co-channel interference and collision behaviour.
- Audible feedback detectability in representative campus noise.
- LCD readability and button usability for drivers and station maintainers.

## 6. Acceptance criteria

The initial prototype is acceptable for supervised demonstration only when:
1. Both MCU target builds compile reproducibly.
2. Two or more stations and two or more carts can be uniquely configured.
3. A student can select a valid destination and submit a request.
4. A cart receives and queues the request exactly once.
5. FIFO order is preserved within that cart.
6. All four driver status stages are reported and displayed correctly.
7. Cancellation and completion are correlated to the correct request.
8. Lost packets, duplicates, reboot and full-queue cases produce safe, explicit outcomes.
9. The two-button limit and LCD/buzzer feedback are verified on physical prototypes.
10. RF and electrical testing is completed using the correct legal radio configuration.
11. No vehicle control is connected to the prototype.
