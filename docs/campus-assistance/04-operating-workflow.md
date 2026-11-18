# 04 — Operating Workflow, Interfaces and State Machines

## 1. Operating model

The student creates a request at a fixed station. All carts listen and maintain local pending lists of unaccepted requests. A driver explicitly controls whether their cart may claim a request. A cart serves one accepted request at a time; it does not build a service queue of multiple accepted rides.

The two physical buttons are context-sensitive. The 16×2 LCD presents the current screen/action, and the buzzer confirms important events. The exact short/long-press mapping must be finalized through usability testing, but no interface may exceed two physical buttons.

## 2. Student station workflow

1. Student activates the station interface.
2. Station guides destination selection using the agreed two-button interaction and audible cues.
3. Student confirms a registered destination.
4. Station validates the destination and creates a unique request ID.
5. Station broadcasts the request and starts its 10-minute default validity timer.
6. Carts that hear the request may add it to their local unaccepted pending lists.
7. The station waits for a committed cart assignment, not merely a packet-reception ACK.
8. Once a cart is assigned, the station announces/displays the assignment/received state.
9. The station relays driver status updates as they are confirmed.
10. Student may cancel while cancellation is permitted.
11. The station reports completion, cancellation, expiry or communication uncertainty accurately.

If no cart is assigned within 10 minutes, the station expires the request and informs the student. The student may submit a new request.

## 3. Request lifecycle states

| State | Meaning | Initiator / authority |
|---|---|---|
| CREATED | Station has created the request and started its timer. | Origin station |
| PENDING_UNASSIGNED | Request is live and available to be claimed; carts may hold local copies. | Origin station |
| CLAIM_PENDING | A cart has sent a claim; no assignment is yet confirmed. | Claiming cart |
| ASSIGNED / RECEIVED | Station has committed one cart and the matching assignment confirmation is acknowledged. | Origin station + winning cart |
| ON_THE_WAY | Driver has indicated departure toward pickup. | Driver via cart |
| ARRIVED_AT_PICKUP | Driver has reached pickup and manually confirmed. | Driver via cart |
| IN_JOURNEY | Optional internal state after pickup; may be represented without a separate student-facing label. | Driver/workflow |
| COMPLETED | Driver confirms the student was dropped at destination. | Driver via cart |
| CANCELLED | Student cancellation is committed by the station and propagated. | Student via station |
| EXPIRED | Ten-minute unaccepted-request lifetime elapsed at the origin station. | Origin station |
| COMMUNICATION_UNCERTAIN | A critical transition cannot be confirmed. | Any node detecting timeout |

Normal service sequence:
`CREATED → PENDING_UNASSIGNED → CLAIM_PENDING → ASSIGNED/RECEIVED → ON_THE_WAY → ARRIVED_AT_PICKUP → COMPLETED`

Alternative terminal outcomes: `CANCELLED` or `EXPIRED`. Do not let a late packet overwrite a terminal state.

## 4. Driver availability state machine

Driver availability is separate from the request lifecycle.

| Driver/cart mode | Receive requests | Add valid unassigned requests to local pending list | Claim | Active assignment |
|---|---|---|---|---|
| NOT_ACCEPTING | Yes | Yes, after any disable-time clear | No | No |
| ACCEPTING_IDLE | Yes | Yes | Yes, oldest eligible request only | At most one |
| SERVING | Yes | Yes | No | Exactly one |
| DISABLED_AFTER_DISABLE | Yes | Yes, as a fresh pending list | No | No |

Rules:
- The driver manually enables persistent **Accepting Requests** mode.
- The mode stays enabled until the driver disables it or a request is assigned.
- A cart accepts at most one active assignment.
- Once assigned, it stops claiming other requests and enters SERVING.
- On journey completion, the cart returns to NOT_ACCEPTING; the driver must manually enable acceptance again.
- If the driver disables acceptance before assignment, the cart clears its current local pending list immediately.
- Clearing a cart's local list does not cancel the student request or erase other carts' copies.
- While disabled, the cart continues listening. New requests received after the clear may populate a fresh local pending list, but cannot be claimed until the driver enables acceptance.
- A cart that re-enters ACCEPTING_IDLE examines its local pending requests in FIFO order, skipping entries that are expired, cancelled or assigned elsewhere.

## 5. Two-button station interface

The station has at most two buttons, a 16×2 LCD and a buzzer.

Proposed contextual mapping:
- Button A short press: NEXT destination.
- Button B short press: PREVIOUS / BACK.
- Button A long press: CONFIRM selected destination / submit.
- Button B long press: CANCEL, when a request is active and cancellation is permitted.

This mapping is a design proposal, not yet validated. Destination selection is a key accessibility risk: the buzzer cannot speak destination names. The design must include a tested nonvisual method, such as tactile/Braille destination directory and a consistent audible selection code. LCD text alone is insufficient for a blind student.

Suggested screen flow:
1. Idle / request assistance.
2. Destination selection.
3. Destination confirmation.
4. Sending request.
5. Waiting for cart assignment.
6. Assigned / received.
7. Journey progress.
8. Completed, cancelled, expired or communication uncertain.

Use distinct audible patterns for selection movement, confirmation, assignment, status changes, cancellation, expiry and errors. Patterns must be user-tested in campus noise.

## 6. Two-button cart interface

The cart has at most two physical buttons, one 16×2 LCD and a buzzer. Buttons are context-sensitive and must not allow accidental skipping of required journey stages.

Required driver actions:
- Enable Accepting Requests.
- Disable Accepting Requests.
- Confirm/advance the active request through the required journey stages.
- Manually re-enable acceptance after completion.

Proposed interface principles:
- LCD always shows the cart mode and the next valid action.
- A deliberate long press is used for mode changes or irreversible actions.
- Short presses navigate/inspect; confirmation is separate where practical.
- The same button may have different meanings on different screens, but the LCD must state the action before it is accepted.
- Buzzer confirms mode changes, accepted commands and invalid actions.
- Driver interaction should occur only when safely stopped; no control action should require looking at the LCD while driving.

The exact button mapping is intentionally left open until a state-by-state interaction table is prototyped. The two-button limit is fixed; the mapping is not.

## 7. Request queue and assignment behavior

The cart has two distinct data concepts:
- **Active assignment:** zero or one request accepted by this cart and currently being served.
- **Pending list:** zero or more unaccepted/unassigned request references heard by this cart.

Rules:
1. Pending list order is FIFO per cart.
2. Merely receiving a request does not assign it.
3. The cart may claim only when driver acceptance is enabled and there is no active assignment.
4. The oldest eligible pending request is attempted first.
5. Successful station-confirmed assignment removes the request from the winning cart's pending list and creates its active assignment.
6. Other carts remove that request from their pending lists when they receive the assignment announcement.
7. No cart may claim a second request while serving.
8. If the queue is full, the cart must not overwrite an entry. It sends an explicit capacity response; station retry/rebroadcast policy must preserve request availability to other carts.
9. Expired/cancelled/already-assigned entries are removed or marked terminal without reordering the remaining live entries.
10. When the driver disables acceptance, clear the local pending list. Do not send a student cancellation.

## 8. Ten-minute request expiry

- Default: **10 minutes** from creation at the originating station.
- Firmware configurable; do not bake 10 minutes into the packet schema.
- Station is authoritative for the expiry deadline.
- A cart may locally age entries as a cleanup optimization, but it must reject/avoid claims if the station reports expiry.
- The station broadcasts expiry so carts can remove the request.
- A claim received after expiry is rejected.
- Expiry applies to unaccepted/unassigned requests. Once a cart is assigned, the request follows the active journey/cancellation rules rather than the pending-request expiry timer.

## 9. Cancellation behavior

- Student cancellation is sent by the origin station with the full request identity.
- All carts that have the request in pending lists remove it.
- If assigned, cancellation is forwarded to the winning cart and shown to the driver.
- If cancellation races with assignment, station-side ordering decides whether assignment or cancellation committed first.
- A cancellation ACK confirms firmware processing, not physical stopping or human awareness.
- The driver follows the institution's safe operating procedure if cancellation arrives after movement begins.
- The system never controls vehicle motion.

## 10. Communication loss and restart

- Critical messages use bounded retries and explicit acknowledgement.
- If an assignment is not confirmed, the cart must not assume it won.
- If status ACK is missing, show “Status not confirmed” or “Communication uncertain”.
- Retransmit the same request identity; do not create a new student request merely because an ACK was lost.
- After reboot, recover or explicitly flag any uncertain active assignment before processing new claims.
- Do not silently reassign a request whose ownership is ambiguous.

## 11. Audible feedback

Illustrative pattern concepts:
- Selection moved: one short beep.
- Local request transmitted: one confirmation tone.
- Cart assignment confirmed: two short beeps.
- On the way: one long beep.
- Arrived at pickup: three short beeps.
- Completed: long-short-long.
- Cancelled: two low-pitch beeps.
- Expired: long warning tone.
- Error / no acknowledgement: repeated short warning pattern.

Exact frequencies, durations and sequences are not frozen. Test audibility, distinctiveness and user interpretation with intended users and representative campus noise.
