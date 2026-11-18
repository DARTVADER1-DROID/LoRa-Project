# 04 — Operating Workflow and State Machines

## 1. User journey

Example: a student is at the Library and wants to travel to the Canteen.

1. Student uses the Library station.
2. Station announces/indicates the destination-selection mode.
3. Student navigates the registered destination list using the two-button interaction.
4. Student confirms Canteen.
5. Station creates and broadcasts a request with origin=Library and destination=Canteen.
6. The request is received by eligible carts and enters the cart queue/ownership process.
7. Once one cart is assigned, the station reports that the request has been received/assigned.
8. Driver progresses through the status sequence.
9. Student hears the station's audible updates and can cancel if the request is still cancellable.
10. Driver marks the journey completed after dropping the student at the destination.
11. The request is acknowledged and retired.

## 2. Required journey states

The agreed user-facing stages are:

| State | Meaning | Who initiates it |
|---|---|---|
| RECEIVED | A cart has received and taken responsibility for the request. This must be backed by a matching protocol acknowledgement/assignment. | Cart/assignment protocol |
| ON_THE_WAY | Driver has indicated that the cart is travelling toward the pickup station. | Driver |
| ARRIVED_AT_PICKUP | Driver has reached the pickup station and manually confirms arrival. | Driver |
| COMPLETED | Driver has dropped the student at the destination and manually confirms completion. | Driver |

Do not use one ambiguous “Arrived” state for both pickup and destination. The two-button interface must provide distinct actions for **Arrived at pickup** and **Complete journey**.

## 3. Request lifecycle

Recommended request state model:

```
CREATED
   |
   v
PENDING_DISPATCH
   |
   v
RECEIVED / ASSIGNED
   |
   v
ON_THE_WAY
   |
   v
ARRIVED_AT_PICKUP
   |
   v
IN_JOURNEY (optional internal state)
   |
   v
COMPLETED
```

Alternative exits:
- CREATED or PENDING_DISPATCH → CANCELLED_BY_STUDENT
- RECEIVED / ASSIGNED → CANCELLED_BY_STUDENT (subject to race resolution)
- PENDING_DISPATCH → EXPIRED
- Any active state → COMMUNICATION_UNCERTAIN when critical status/ownership cannot be confirmed.

The **IN_JOURNEY** state is recommended internally after pickup arrival, even though it is not one of the four primary status labels requested. If the student-facing status set must remain exactly four labels, this internal state can be omitted and the driver can use a completion action after pickup arrival. The implementation must still distinguish the pickup-arrival event from completion.

## 4. Two-button station interaction — target proposal

A two-button interface can support destination selection without extra controls. Proposed mapping:

- **Button A — NEXT / SELECT:** short press moves to the next registered destination; long press confirms the highlighted destination.
- **Button B — BACK / CANCEL:** short press moves backward or returns to the previous screen; long press cancels the current request when cancellation is allowed.

The exact mapping must be user-tested. It is a proposed interaction model, not yet validated with blind or visually impaired participants.

Suggested screen flow:
1. Idle: “Request assistance”.
2. Destination list: one destination shown at a time, with an audible cue on each change.
3. Confirmation: “To: CANTEEN / Hold A to send”.
4. Sending: “Sending request”.
5. Pending: “Request sent / Waiting for cart”.
6. Assigned: “Cart received”.
7. Status: audible patterns for received, on the way, arrived at pickup and completed.
8. Cancellation confirmation: use a deliberate long press and a distinct cancel tone to reduce accidental cancellation.

Since students may not be able to read the LCD, each screen change and important state must have a distinguishable audible pattern. The buzzer is not a speech synthesizer; destination names need a nonvisual selection strategy such as counted/unique beep patterns, tactile station list, or assistance during initial use. This accessibility issue is a key usability test, not a solved assumption.

## 5. Two-button cart interaction — target proposal

The cart has a maximum of two buttons. Proposed contextual mapping:

- **Button A — NEXT / CONFIRM:** short press cycles the visible action/request information; long press confirms the current status action.
- **Button B — BACK / STATUS:** short press moves backward/cycles available details; long press opens/executes the contextually permitted secondary action.

An even simpler implementation may assign:
- Button A: advance the active request to the next permitted stage.
- Button B: repeat status/confirm or cancel the active request, depending on the agreed operating policy.

The cart display must show the current request and the next permitted action before the driver confirms it. Avoid accidental stage skipping. Driver actions should be edge-detected, debounced and require a deliberate press duration where appropriate.

## 6. Driver workflow

For each active FIFO request:
1. Cart displays pickup station and destination.
2. Driver confirms **Received** if receipt is not already automatically committed by the protocol.
3. Driver presses the context-appropriate control for **On the way** when departing toward pickup.
4. Driver presses the control for **Arrived at pickup** upon reaching the pickup station.
5. Driver transports the student manually.
6. Driver presses the completion control after dropping the student at the destination.
7. Cart sends the status/completion packet and waits for matching acknowledgement before permanently retiring the transaction.
8. Cart selects the next queued request.

The precise point at which “Received” is entered (automatic on confirmed queue insertion versus driver-confirmed) must be finalized in the implementation decision log. The design should not add a third button to resolve it.

## 7. FIFO queue rules

- FIFO is based on the order in which a cart accepts unique, valid requests into its local queue.
- The active request is not displaced by a newly received request.
- A duplicate packet for an already queued or active request does not create another entry.
- A cancelled request is removed/marked terminal without reordering remaining requests.
- A completed request is retired.
- If the queue is full, the cart must not overwrite the oldest request. It returns a QUEUE_FULL response.
- Queue capacity is a compile-time configurable limit, selected after RAM measurements for each MCU.
- FIFO applies to each cart independently. No claim of campus-wide global FIFO is made.

## 8. Cancellation behavior

Student cancellation:
- Station sends a cancellation message containing the exact request identity and generation.
- If the request is still pending, carts that have queued it remove it.
- If a cart is assigned or already travelling, the cancellation is forwarded to the assigned cart.
- A cancellation acknowledgement confirms MCU processing, not that the driver has physically stopped or that the student is safe.
- If cancellation races with arrival or completion, the station and cart resolve the event using request state and sequence/generation checks. Terminal completion must not be overwritten by a late cancellation.

Operational rule: if a student cancels after a cart has started moving, the driver must receive a clear cancellation notification and follow the institution's safe stopping procedure. The firmware does not control the vehicle.

## 9. Lost communication and restart

- Retry critical messages a finite number of times.
- If acknowledgement is not received, display **Status not confirmed** rather than success.
- Do not create a second request simply because an acknowledgement was lost; retransmit the same request identity.
- After reboot, a node must not silently forget an in-progress assignment if that could lead to duplicate dispatch. Persistence strategy is an open item: EEPROM/flash writes must be minimized and power-loss behavior tested.
- If state cannot be reconstructed safely, show **Needs operator check** and require manual reconciliation before reassigning the student request.

## 10. Audible feedback

Use a small, documented pattern table, for example:
- Request accepted locally: one short beep.
- Cart assignment confirmed: two short beeps.
- On the way: one long beep.
- Arrived at pickup: three short beeps.
- Completed: long-short-long.
- Cancelled: two low-pitch beeps.
- Error / no acknowledgement: repeated short warning pattern.

These are illustrative patterns only. Final frequency, duration and sequence must be measured and user-tested in campus noise. Avoid relying only on pitch if hearing variation or environmental noise makes patterns ambiguous.
