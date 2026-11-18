# Campus Assistance System — Engineering Documentation

**Project type:** Final-year engineering project / research prototype  
**Application:** LoRa-based campus mobility assistance for blind and visually impaired students  
**Status:** Requirements and target design specification. Not a claim that every described feature is already implemented or validated.

## Document map

| Document | Purpose |
|---|---|
| [01 — Project definition](01-project-definition.md) | Background, problem, objectives, scope and contribution |
| [02 — Requirements](02-requirements.md) | Functional, non-functional and interface requirements |
| [03 — System architecture](03-system-architecture.md) | Nodes, roles, topology, data flow and deployment model |
| [04 — Operating workflow](04-operating-workflow.md) | Student, station and driver workflows; state machines |
| [05 — Hardware design](05-hardware-design.md) | Arduino Uno, ESP32, RA-02, LCD, buttons, buzzer and electrical constraints |
| [06 — LoRa communication design](06-lora-communication-design.md) | Radio constraints, network design, packet concepts and RF validation |
| [07 — Firmware and data model](07-firmware-and-data-model.md) | Dedicated MCU firmware, IDs, FIFO queues, event handling and persistence |
| [08 — Safety, privacy and accessibility](08-safety-accessibility.md) | Human factors, operational boundaries and risk controls |
| [09 — Verification and research plan](09-verification-research-plan.md) | Test matrix, metrics, experiments and paper/patent preparation |
| [10 — Decisions and open items](10-decisions-and-open-items.md) | Traceable decisions, implementation gaps and unresolved engineering choices |

## Reading convention

- **Confirmed requirement** means explicitly agreed during project discussion.
- **Target design** means the recommended design to implement; it may differ from the current prototype code.
- **Existing implementation** refers to behavior found in the repository at the time this documentation was prepared.
- **Open item** means evidence or a design decision is still required.
- No unmeasured range, capacity, battery life, reliability, cost or accessibility outcome is presented as a result.

## Important implementation distinction

The repository currently contains an earlier campus prototype whose source has a different interface and workflow in places (for example, three cart buttons, one station button, an explicit cart accept action, and a bounded queue). This documentation captures the newly agreed target: at most two buttons on each node, station and cart interfaces using a 16×2 LCD and buzzer, firmware-level device registration, automatic request queuing and FIFO handling. The source firmware must be deliberately revised and tested before it can be said to conform.

## System design at a glance

- Fixed campus stations are installed at registered locations such as the library, mess, canteen and departments.
- A student requests a ride from a station and selects any other registered station as destination.
- Stations and electric carts communicate using raw LoRa radio only; no Wi-Fi, cellular, Bluetooth, GPS or cloud service is part of the agreed communication path.
- Station and cart identities are unique firmware-configured IDs, for example `ST001` and `CRT001`.
- Each cart automatically receives valid requests and places them in a FIFO queue. The driver handles the queue sequentially.
- Driver status updates are manual: **Received**, **On the way**, **Arrived at pickup**, and **Journey completed**.
- Student cancellation is supported.
- Each station and cart has a maximum of two physical buttons. A 16×2 LCD and simple buzzer patterns provide feedback.
- Arduino Uno and ESP32 are both target MCU platforms with dedicated firmware builds.
- The initially identified radio is Ai-Thinker RA-02 / Semtech SX1278. Its RF band must be checked against Indian regulatory requirements before field transmission; a common 433 MHz RA-02 is not an 865–867 MHz radio.
