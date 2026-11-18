# Request Sequence Diagram

```mermaid
sequenceDiagram
    actor Student
    participant Station as Origin station
    participant Radio as LoRa broadcast
    participant CartA as Cart A
    participant CartB as Cart B
    actor Driver

    Student->>Station: Select destination and submit
    Station->>Station: Create request ID; start 10 min timer
    Station->>Radio: REQUEST
    Radio-->>CartA: REQUEST
    Radio-->>CartB: REQUEST
    CartA->>CartA: Validate/deduplicate; add to pending FIFO
    CartB->>CartB: Validate/deduplicate; add to pending FIFO
    Note over CartA,CartB: Both listen even if driver is not accepting

    Driver->>CartA: Enable Accepting Requests
    CartA->>CartA: Select oldest eligible pending request
    CartA->>Radio: CLAIM(request ID, Cart A)
    Radio-->>Station: CLAIM
    Station->>Station: Validate expiry; commit at most one winner
    Station->>Radio: ASSIGN(request ID, Cart A)
    Radio-->>CartA: ASSIGN
    Radio-->>CartB: ASSIGN
    CartA->>CartA: Create sole active assignment; remove from pending
    CartB->>CartB: Remove request from pending
    CartA-->>Station: ASSIGN_ACK
    Station-->>Student: Assignment confirmed

    Driver->>CartA: On the way
    CartA->>Radio: STATUS: ON_THE_WAY
    Radio-->>Station: Status update
    Driver->>CartA: Arrived at pickup
    CartA->>Radio: STATUS: ARRIVED_AT_PICKUP
    Radio-->>Station: Status update
    Driver->>CartA: Complete after drop-off
    CartA->>Radio: COMPLETE
    Radio-->>Station: COMPLETE
    Station-->>CartA: COMPLETE_ACK
    CartA->>CartA: Retire active assignment; set NOT_ACCEPTING
    Note over CartA,Driver: Driver must manually enable acceptance again
```

## Additional transitions

- If driver disables acceptance before assignment: clear Cart A's local pending list only; do not cancel the station-owned request.
- Cart A continues listening while disabled. Requests received after the clear may populate a new pending list but cannot be claimed.
- If Cart A is serving a student, it may collect pending requests but cannot claim another.
- If no cart is assigned within 10 minutes, the origin station broadcasts EXPIRED; carts remove the request and reject late claims.
- Student cancellation is a separate station-originated terminal event correlated to the full request identity.
- A cart must not assume it won merely because it sent CLAIM. The station's matching ASSIGN confirmation is required.
- ACKs confirm MCU-level processing, not physical arrival or human awareness.
