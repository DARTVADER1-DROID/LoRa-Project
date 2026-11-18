# 10 — Decisions and Open Engineering Items

## 1. Purpose and status labels

This is the decision register for the Campus Assistance target design.

- **Confirmed**: explicitly selected by the project owner.
- **Engineering proposal**: recommended implementation detail that still needs protocol design, review or testing.
- **Open**: not yet decided or requires evidence.
- **Existing implementation**: behavior found in the repository source; it may differ from the target.

The target design is not a claim that the current firmware already implements these decisions.

## 2. Confirmed project decisions

| ID | Confirmed decision | Status |
|---|---|---|
| D-01 | Main application is campus mobility assistance for blind/visually impaired college students. | Confirmed |
| D-02 | Final-year project, with research-paper work and patent exploration. | Confirmed |
| D-03 | Arduino Uno and ESP32 are both target MCU families, with dedicated firmware builds. | Confirmed |
| D-04 | Use Ai-Thinker RA-02 / SX1278; retain this module subject to applicable Indian WPC compliance verification. | Confirmed with regulatory gate |
| D-05 | Use one common LoRa configuration across all stations and carts. | Confirmed |
| D-06 | Communication path is LoRa only. | Confirmed |
| D-07 | Deployment includes fixed campus stations and electric carts; station locations may include library, mess, canteen and departments. | Confirmed |
| D-08 | IDs are individual, unique and provisioned through firmware initially. | Confirmed |
| D-09 | Maximum two physical buttons per station and per cart. | Confirmed |
| D-10 | Use a 16×2 LCD and buzzer. | Confirmed |
| D-11 | Driver controls whether their cart accepts requests using a persistent Accepting Requests mode. | Confirmed |
| D-12 | Driver must manually enable acceptance; it remains enabled until disabled or an assignment is accepted. | Confirmed |
| D-13 | A cart may hold only one accepted/active assignment at a time. | Confirmed |
| D-14 | Pending lists contain only unaccepted/unassigned requests, not a queue of multiple accepted rides. | Confirmed |
| D-15 | Every cart listens continuously, even when the driver is not accepting requests. | Confirmed |
| D-16 | A cart may collect new pending requests while not accepting, but cannot claim them. | Confirmed |
| D-17 | When the driver disables acceptance, clear that cart's current local pending list. | Confirmed |
| D-18 | Clearing a local list does not cancel the station-owned request or clear another cart's list. | Confirmed interpretation of local-clear decision |
| D-19 | If another cart is assigned a request, other carts remove it from their pending lists when assignment information is received. | Confirmed |
| D-20 | Use automatic claim-and-assignment coordination so only one cart owns a request. | Confirmed |
| D-21 | When an idle driver enables acceptance, process eligible pending requests in FIFO order. | Confirmed |
| D-22 | After journey completion, the cart must remain not accepting until the driver manually re-enables it. | Confirmed |
| D-23 | Unaccepted requests expire after 10 minutes by default. | Confirmed |
| D-24 | Request expiry duration must be configurable in firmware. | Confirmed |
| D-25 | Student can cancel a request. | Confirmed |
| D-26 | Driver manually reports On the way, Arrived at pickup and Journey completed; Received/Assigned is backed by the assignment protocol. | Confirmed workflow |
| D-27 | No autonomous driving, steering, braking, throttle or traction-control functionality. | Confirmed scope boundary |

## 3. Confirmed request and cart behavior

The following rules are the consolidated target behavior:

1. A station creates a request with a unique identity and starts the default 10-minute expiry timer.
2. Every cart listens and may add a live unassigned request to its local FIFO pending list, even if its driver is not accepting.
3. A pending entry is not an assignment and does not mean the driver has accepted it.
4. A cart claims only when its driver has enabled acceptance and the cart has no active assignment.
5. When the driver enables acceptance, the cart examines its pending list in FIFO order and attempts the oldest eligible request.
6. The station commits at most one winning cart for a request and broadcasts the assignment.
7. The winning cart stores one active assignment; other carts remove that request from their local pending lists when they receive the assignment announcement.
8. While serving, a cart cannot claim another request. It may continue receiving and storing unassigned requests in its pending list.
9. If the driver disables acceptance, the cart clears its current local pending list. It continues listening; requests received after the clear can populate a new pending list.
10. Completion does not automatically restore acceptance. Driver must manually enable it again.
11. Unaccepted requests expire 10 minutes after station creation by default. The originating station is authoritative and rejects late claims.
12. Local pending-list clearing is not student cancellation. Only the originating station's cancellation event changes the request's global lifecycle.

## 4. Engineering proposals requiring detailed design

| ID | Proposal | Why / remaining work |
|---|---|---|
| P-01 | Use full request identity: origin station ID + sequence + generation/session. | Prevent duplicates and stale packet confusion. |
| P-02 | Origin station is authoritative for request expiry and assignment commitment. | Avoid inconsistent ownership and clock decisions. |
| P-03 | Use idempotent CLAIM, ASSIGN, ASSIGN_ACK, CANCEL and EXPIRED messages. | Recover safely from lost/duplicate packets. |
| P-04 | Use bounded pending lists and a separate zero-or-one active assignment record. | Correctly model the user's one-active-ride requirement. |
| P-05 | Use context-sensitive short/long press actions on two buttons. | Preserve low-cost hardware and avoid extra controls. |
| P-06 | Use non-blocking button/radio state machines. | Keep RF reception responsive. |
| P-07 | Use bounded randomized backoff or deterministic claim slots for simultaneous claims. | Reduce collisions; exact scheme requires testing. |
| P-08 | Retain request tombstones/rebroadcast or reconciliation so carts that miss ASSIGN/EXPIRED/CANCEL events eventually clean their pending lists. | Broadcast loss can leave stale local copies. |
| P-09 | Show communication uncertain/not confirmed instead of claiming an unacknowledged transition succeeded. | Prevent misleading feedback. |
| P-10 | Keep Uno and ESP32 on one protocol while isolating board-specific drivers. | Mixed-MCU interoperability. |

## 5. Open engineering decisions

### Hardware and regulatory
1. Which exact ESP32 board/module will be used?
2. Confirm the exact RA-02 variant, antenna and supported RF characteristics.
3. Verify Indian WPC rules applicable to the selected 433 MHz module, antenna, EIRP, bandwidth, duty cycle and channel access before any campus transmission. The module selection is not itself regulatory approval.
4. What power source will each station use?
5. What auxiliary voltage is available on each cart?
6. Is the LCD I2C or parallel? What is the backpack logic voltage?
7. Active or passive buzzer, and what drive circuit/current?
8. Enclosure, mounting and weather protection requirements?

### Accessible station interface
9. How will a blind student identify the destination without spoken announcements? Evaluate tactile/Braille directory, deterministic audible codes or a combined method with intended users.
10. Confirm exact station button mapping and selection timeout.
11. How should the station prevent accidental submissions or cancellation?

### Cart interface and operating policy
12. Freeze the exact two-button state/action table for Enable, Disable, On the way, Arrived at pickup and Completed.
13. Decide whether Received is automatically displayed on confirmed assignment or requires a separate driver confirmation. The assignment protocol must not be confused with proof the driver noticed it.
14. Define how the driver is notified about a student cancellation while serving.
15. Define safe manual recovery if the cart loses power or radio contact during an active assignment.

### Protocol and algorithm
16. Freeze packet field widths, protocol version, request identity, message types and maximum payload.
17. Define exact claim collision/backoff, station commit and assignment retry rules.
18. Define how carts that miss an assignment/expiry/cancellation broadcast reconcile their pending lists.
19. Define queue capacity separately for Uno and ESP32 after memory profiling.
20. Define retry limits, request rebroadcast interval, ACK timeouts and maximum transaction duration.
21. Define active-assignment persistence and power-loss recovery.
22. Define authentication/replay protection appropriate to the research prototype.

### Validation and research
23. Choose initial station/cart count for demonstration.
24. Select campus RF test locations and record distances/path conditions.
25. Define packet delivery, assignment latency, status latency and collision acceptance thresholds after baseline measurements.
26. Arrange accessibility testing, consent and institutional approvals.
27. Conduct prior-art review and institutional IP review before public disclosure if patent filing is intended.

## 6. Existing source versus target

The current campus firmware is an earlier prototype and does not yet implement the newly confirmed target. Earlier source review found:
- One shared campus sketch compiled as station or cart.
- Station UI differs from the target two-button/16×2 design.
- Cart UI had three physical buttons.
- Existing cart queue and explicit accept behavior differ from the new pending-list plus driver-mode model.
- Existing arbitration was not yet the fully specified station-authoritative claim/assignment algorithm.
- Existing packet format requires review for protocol versioning and request generation identity.
- A 433 MHz bench default is not proof of legal authorization for field use.

Do not describe these target features as implemented until firmware is changed and tests pass.

## 7. Recommended engineering sequence

1. Resolve exact ESP32 board and radio regulatory gate.
2. Freeze station and cart button/action tables.
3. Test nonvisual destination selection.
4. Specify packet schema and request identity.
5. Specify claim collision, station commit, assignment and reconciliation.
6. Implement driver mode, one active assignment and local pending-list behavior.
7. Implement station-authoritative 10-minute expiry and cancellation races.
8. Add host-side state-machine and packet tests.
9. Run Uno↔ESP32 mixed-node tests.
10. Perform RF, queue, power, accessibility and supervised field tests.
11. Update documentation with measured evidence and firmware revision.
12. Prepare paper and patent prior-art review.
