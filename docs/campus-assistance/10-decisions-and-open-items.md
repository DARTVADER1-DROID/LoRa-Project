# 10 — Decisions and Open Engineering Items

## 1. Confirmed decisions from project discussion

| ID | Decision | Status |
|---|---|---|
| D-01 | Main focus is campus assistance for blind/visually impaired college students. | Confirmed |
| D-02 | Intended as a final-year project with research paper and patent exploration. | Confirmed |
| D-03 | The repository should contain a separate, detailed project documentation suite. | Confirmed |
| D-04 | Arduino Uno and ESP32 are both target MCU platforms. | Confirmed |
| D-05 | Dedicated firmware builds are required for both MCU families. | Confirmed |
| D-06 | Identified radio is Ai-Thinker RA-02 / SX1278. | Identified; band/regulatory suitability unresolved |
| D-07 | System consists of stations and electric carts. | Confirmed |
| D-08 | Stations may be installed at library, mess, canteen, departments and other campus locations. | Confirmed |
| D-09 | Communication is LoRa only. | Confirmed |
| D-10 | Primary workflow is station-to-cart assistance request. | Confirmed |
| D-11 | User-facing progress includes Received, On the way and Arrived; completion is a separate driver action. | Confirmed |
| D-12 | Driver manually operates status controls. | Confirmed |
| D-13 | Student can cancel a request. | Confirmed |
| D-14 | Each cart has a FIFO request queue; keep logic simple. | Confirmed |
| D-15 | Any registered station can be selected as a destination. | Confirmed |
| D-16 | Station count and cart count should be scalable, not fixed to one device. | Confirmed |
| D-17 | Initial station/cart registration is firmware-level only. | Confirmed |
| D-18 | IDs are unique firmware-configured values, e.g. ST001, CRT001. | Confirmed |
| D-19 | Maximum two physical buttons on each station and each cart. | Confirmed |
| D-20 | Use 16×2 LCD. | Confirmed |
| D-21 | Use buzzer with simple beep patterns, not spoken announcements. | Confirmed |

## 2. Target design decisions made in this documentation

These are engineering proposals to make the confirmed requirements implementable. They should be reviewed during implementation.

| ID | Proposal | Reason |
|---|---|---|
| P-01 | FIFO is per cart, based on accepted request order. | No central scheduler or synchronized global clock was specified. |
| P-02 | Use a request identity tuple containing origin station, sequence and generation. | Prevent duplicates and stale packets. |
| P-03 | Use a claim/assignment handshake so only one cart services a broadcast request. | Avoid duplicate dispatch when multiple carts hear one request. |
| P-04 | Use context-dependent short/long press actions for the two buttons. | Satisfy two-button maximum. |
| P-05 | Show “not confirmed” on timeout rather than claiming successful delivery. | Avoid misleading the student. |
| P-06 | Keep the system separate from vehicle motion/control. | Safety boundary. |
| P-07 | Use firmware configuration for the station registry and unique IDs. | Matches initial provisioning decision. |
| P-08 | Keep Uno and ESP32 protocol-compatible but use board-specific hardware configurations. | Cross-platform interoperability. |

## 3. Open engineering decisions

These must be resolved before final firmware and hardware freeze.

### Hardware
1. Which exact ESP32 development board/module will be used?
2. Is the RA-02 SX1278 module fixed, or can the radio module be changed if Indian regulatory band requirements make it unsuitable?
3. What is the selected legal RF band and permitted modem/power profile?
4. What power source will each station use?
5. What auxiliary power source and voltage range are available on the electric cart?
6. Is the 16×2 LCD I2C or parallel? If I2C, what backpack and logic voltage?
7. Active or passive buzzer? Exact current and drive circuit?
8. What enclosure/weather protection is required?

### Student station interface
9. How will a blind student identify the destination while cycling through the list, given that voice announcements are excluded? Counted beep codes, tactile destination directory or another approach must be tested.
10. Should button A short-press move to next destination and long-press confirm, with button B for back/cancel? This is the proposed mapping, not a validated user preference.
11. Is there a required timeout if the student leaves the station during selection?

### Cart interface and operations
12. Should the cart automatically mark a request RECEIVED when it is successfully enqueued, or should the driver explicitly confirm receipt?
13. What exact two-button mapping should the driver use for On the way, Arrived at pickup and Completed?
14. At what safe points should the driver operate the controls?
15. Can a driver cancel a queued request, or only the student cancel it?
16. What happens if the cart becomes unavailable or powers off? The user asked to simplify hardware and avoid extra controls; an explicit low-cost operating policy is still needed.

### Network and protocol
17. What is the desired initial number of stations and carts for demonstration?
18. What maximum queue capacity should be targeted on Uno and ESP32 after RAM profiling?
19. What is the desired request expiry time?
20. How should simultaneous cart claims be arbitrated and collisions reduced?
21. Should station/cart heartbeat be retained, and what should a heartbeat timeout mean?
22. What persistence is required after power loss?
23. What authentication/replay protection is required for the research prototype?

### Validation
24. What campus locations and distances are available for RF testing?
25. What is the acceptance target for packet delivery and status latency?
26. What accessibility testing participants and institutional approvals can be arranged?

## 4. Existing source versus target requirements

The current repository source is an earlier prototype, not yet aligned with every confirmed decision in this suite. In particular, the fetched implementation report/source described:
- One shared campus sketch compiled as station or cart.
- Station uses one request/cancel button and RGB LED/buzzer; it did not show the newly agreed 16×2 station display/two-button interface.
- Cart source had separate accept, cancel and complete controls (three buttons), plus LCD and buzzer.
- Cart queue was bounded (default 10).
- The cart required an explicit accept-button press to claim the queue head.
- Existing station arbitration used a claim window and rotating cart-ID preference, rather than a fully specified simple first-winner FIFO dispatch.
- Existing campus packet structure did not include an explicit protocol-version field.
- Existing source used a 433 MHz bench default and must not be treated as a regulatory frequency decision.

These are implementation gaps to reconcile. Do not claim the new two-button system or the final target workflow is already implemented until firmware is revised and tests pass.

## 5. Recommended next engineering sequence

1. Confirm exact RA-02/module and legal band decision.
2. Confirm exact ESP32 board.
3. Freeze station and cart two-button interaction tables.
4. Resolve nonvisual destination selection.
5. Define the normative packet schema and versioning.
6. Define the claim/assignment and per-cart FIFO behavior in state diagrams.
7. Define Uno and ESP32 pin maps and power rails.
8. Implement firmware in small increments with host-side protocol tests.
9. Run mixed-MCU interoperability tests.
10. Perform RF, power, queue and accessibility experiments.
11. Update this document with measured results and revision history.
12. Prepare research paper and patent prior-art review from evidence, not assumptions.
