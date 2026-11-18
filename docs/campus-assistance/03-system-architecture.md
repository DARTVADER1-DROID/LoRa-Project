# 03 — System Architecture

## 1. Architecture summary

The system is a distributed, raw-LoRa, point-to-multipoint application made from fixed station nodes and mobile cart nodes. There is no central server, internet dependency or non-LoRa communication link in the agreed prototype.

Each station creates and owns its local student request. Each cart listens for eligible requests, stores them in its own bounded FIFO queue and services them one at a time. Stations and carts exchange request, assignment/ownership, status, cancellation and acknowledgement messages.

**Important queue interpretation:** FIFO is guaranteed within each cart's local queue. With no central scheduler and no shared clock, the system cannot claim one globally ordered FIFO queue across every cart and every station. A request may be heard by more than one cart; a simple ownership/claim handshake is required to prevent two carts from servicing the same request. This must be specified and tested before multi-cart deployment.

## 2. Logical components

### 2.1 Station node

Responsibilities:
- Identify itself using a firmware-configured station ID.
- Store the configured station registry used for destination selection.
- Present an accessible destination-selection interface.
- Create a unique request identifier and broadcast the request.
- Receive cart reception/ownership acknowledgements.
- Display and announce meaningful request state.
- Send cancellation for the active request.
- Reject invalid destinations and stale request events.

### 2.2 Cart node

Responsibilities:
- Identify itself using a firmware-configured cart ID.
- Listen for valid requests on the configured campus network.
- Validate request fields and destination IDs.
- Deduplicate requests.
- Enqueue eligible requests automatically.
- Preserve FIFO order in its local queue.
- Coordinate request ownership so that only one cart is assigned.
- Display the active request and destination.
- Let the driver report Received, On the way, Arrived at pickup and Journey completed.
- Send status and completion acknowledgements.
- Remove or mark completed/cancelled requests without corrupting queue order.

### 2.3 Human roles

- Student interacts with the pickup station.
- Driver interacts with the cart console and physically operates the vehicle.
- Administrator configures firmware IDs, radio settings and station registry entries during commissioning.

## 3. Network topology

Conceptual topology:

```
                LoRa broadcast domain (same configured RF profile)
       +-------------------+       +-------------------+
       | Station ST001     |       | Station ST002     |
       | Library            |       | Canteen            |
       +---------+---------+       +---------+---------+
                 \                         /
                  \                       /
                   +----------+-----------+
                              |
                    +---------+---------+
                    | Shared LoRa medium |
                    +---------+---------+
                              |
             +----------------+----------------+
             |                                 |
    +--------+---------+              +--------+---------+
    | Cart CRT001      |              | Cart CRT002      |
    | FIFO queue       |              | FIFO queue       |
    +------------------+              +------------------+
```

This is a conceptual broadcast domain, not a guarantee of full connectivity. Buildings, walls, vehicle bodies, antenna placement, interference and radio settings affect actual coverage.

## 4. Device identity and configuration

Use human-readable labels and numeric protocol identifiers:

| Device class | Example label | Example numeric ID |
|---|---|---:|
| Station | ST001 | 1 |
| Station | ST002 | 2 |
| Cart | CRT001 | 1001 |
| Cart | CRT002 | 1002 |

The exact numeric allocation is a design convention to finalize in firmware. It must avoid collisions and remain within the selected packet field width.

Each station is provisioned with:
- Its own unique station ID.
- A location label, e.g. LIBRARY.
- The list of valid station IDs and display labels.
- Campus network ID and compatible protocol version.
- RF profile selected for the actual module and local regulations.

Each cart is provisioned with:
- Its own unique cart ID.
- Campus network ID and compatible protocol version.
- RF profile and queue capacity.
- Any static station registry data needed to validate destination IDs.

**Firmware-level registration:** no button-driven registration menu, PIN entry or over-the-air enrollment is included in the initial version. Adding a device means editing the relevant configuration, compiling/flashing the correct firmware and commissioning the unique ID. Future interface-based provisioning is outside the initial scope.

## 5. Request flow

1. Student chooses a destination at station STxxx.
2. Station validates that destination against its configured station registry.
3. Station creates a unique request tuple, preferably including origin station ID, request sequence and generation/session discriminator.
4. Station broadcasts the request.
5. Eligible carts validate and deduplicate the request.
6. Cart(s) may queue the request provisionally; an ownership handshake selects one servicing cart.
7. The selected cart sends a confirmed receipt/assignment response.
8. Station reports that a cart has received/been assigned the request only after matching acknowledgement.
9. Driver handles the active request and reports each status transition.
10. Station receives and displays status updates.
11. Completion or cancellation is acknowledged and the request is retired from the active queue.

## 6. Multi-cart ownership — required simple rule

A pure broadcast request heard by every cart must not cause every cart to transport the same student. The implementation therefore needs a minimal claim/assignment mechanism.

Recommended first prototype:
- Each cart queues the request as **unassigned/provisional**.
- A cart may claim only the head request it is ready to service.
- The originating station accepts the first valid claim it commits for that request and broadcasts/returns an assignment confirmation identifying the winning cart.
- Other carts remove or mark the request as assigned elsewhere when they receive the confirmation.
- Claims and assignment confirmations are idempotent and include the full request identity.
- If the claim/assignment exchange times out, the request remains unresolved; the station must not display a confirmed cart assignment.

This is a simple first-winner arbitration proposal, not a confirmed requirement from the discussion. It needs careful collision testing because simultaneous cart claims can collide. A short randomized response delay or deterministic slot can reduce collisions but cannot guarantee collision-free access. The exact policy is an open design item in document 10.

## 7. Network scaling

“Scalable” means firmware and packet formats are not designed around a single hardcoded station/cart count. It does not mean unlimited devices.

Actual limits arise from:
- Numeric ID width and registry storage.
- Uno SRAM and flash.
- Maximum packet airtime and regulatory duty-cycle.
- Broadcast collision probability and total traffic.
- Queue memory per cart.
- Number of active and stale device records.
- RF coverage and hidden-node effects.

A measured supported device count must be published after load testing. Do not advertise unlimited scalability.

## 8. MCU variants

Arduino Uno and ESP32 are both target platforms, with dedicated firmware builds. A shared protocol specification should keep the over-the-air behavior compatible, but implementation details may differ:
- Pin mapping and peripheral drivers.
- Logic voltage and power supply.
- Available RAM/flash and queue capacity.
- EEPROM emulation or nonvolatile storage method.
- Timing and task scheduling.

The same protocol version and compatible RF/modem settings are required for nodes to communicate. A packet schema change requires coordinated firmware updates and protocol version management.

## 9. Failure boundaries

- A station can confirm packet reception only when it receives a matching acknowledgement.
- A cart can confirm request receipt only after validating and storing the request.
- A radio acknowledgement does not prove the driver saw the display.
- A status packet does not prove the vehicle physically reached a location.
- Loss of heartbeat does not prove a cart has stopped.
- A request should be marked **communication uncertain**, not silently reassigned, when ownership or journey state is ambiguous.

The project must keep these distinctions visible in both the protocol and the UI.
