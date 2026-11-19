# Firmware Low-Level Design (LLD)

**Status:** Interface specification; translated into a C++/Arduino stub skeleton (not operational firmware).
**Targets:** Station and cart firmware; Arduino Uno and a tentative ESP32 board.
**Authority:** Refines documents 01–10. Confirmed requirements and decision register take precedence.

## Purpose

This folder is the implementation blueprint. It defines layers, responsibilities, class contracts, function signatures, state ownership, error behavior and test seams. It intentionally contains no completed implementation, pin assignments, register settings or selected RF parameters.

## Documents

1. [Architecture and layers](01-architecture-and-layers.md)
2. [Class and interface catalogue](02-class-and-interface-catalogue.md)
3. [Function contracts and state machines](03-function-contracts-and-state-machines.md)
4. [Protocol and distributed coordination](04-protocol-and-distributed-coordination.md)
5. [Memory, portability and verification](05-memory-portability-and-verification.md)
6. [SOLID boundaries and scalability contracts](06-solid-and-scalability-contracts.md)

## Code skeleton

The LLD contracts have been laid out as an organized C++/Arduino skeleton in [firmware/campus_assistance](../../../firmware/campus_assistance/README.md). The scaffold separates portable contracts, hardware ports, role use cases and four role/MCU sketch entry points. Runtime implementations and target adapters remain intentionally pending. The host CMake target checks that public headers compile as C++11 contracts; it does not claim operational firmware or target hardware build readiness.

## Normative constraints

- Raw LoRa only; all nodes use one common RF profile and compatible over-the-air protocol.
- Retain Ai-Thinker RA-02 / SX1278, subject to the Indian regulatory verification gate.
- Every cart continuously listens and maintains a local FIFO of live, unaccepted/unassigned requests.
- Driver acceptance is explicit and persistent until disabled or an assignment is committed.
- A cart owns at most one active assignment.
- Disabling acceptance clears that cart's local pending list only. It does not cancel station-owned requests or alter other carts' lists.
- Requests received after the clear may populate a fresh local pending list even while not accepting.
- Assignment is station-authoritative; transmitting CLAIM does not make a cart the winner.
- Other carts remove an assigned request when they learn of the station's committed assignment; missed broadcasts require reconciliation.
- Completion returns the cart to NOT_ACCEPTING; manual reactivation is required.
- Unaccepted request lifetime defaults to 10 minutes (600,000 ms), configurable in firmware.
- Maximum two physical buttons per node; 16×2 LCD and buzzer.
- Firmware provisioning initially; no runtime enrollment/password UI.
- No vehicle actuation or safety-critical vehicle control.

## Architecture conformance

The normative SOLID and scalability rules are in [06 — SOLID Boundaries, Ownership and Scalability Contracts](06-solid-and-scalability-contracts.md). In particular, the station claim coordinator is the only assignment-commit authority, the request repository stores complete records and provides an atomic assign-if-unassigned guard, and both role applications use explicit dependency injection.

## Conventions

Signatures are C++ contracts. Status and Result-like return values represent explicit success/error outcomes. RequestId always means the complete identity tuple. Capacities and timing values are validated configuration, not magic constants. Functions that service radio, UI, buttons or timers must be non-blocking or explicitly bounded. Avoid dynamic allocation in time-critical paths.

## Deliberately open

Exact ESP32 board, pin maps, RF legal settings, wire field widths, numeric message IDs, retry/timing values other than the 10-minute default, queue capacities, persistence layout and final two-button action mapping remain open.

## Implementation rule

Implement one module at a time against these contracts. Add tests before hardware integration. Public contract changes must update this LLD, protocol vectors and both MCU targets together.
