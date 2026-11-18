# Request Sequence Diagram

```mermaid
sequenceDiagram
    actor Student
    participant Station
    participant Radio as LoRa network
    participant Cart as Selected cart
    actor Driver

    Student->>Station: Select destination and submit
    Station->>Station: Validate destination and create request ID
    Station->>Radio: REQUEST (broadcast)
    Radio-->>Cart: REQUEST
    Cart->>Cart: Validate, deduplicate and enqueue FIFO
    Cart-->>Radio: REQUEST_ACK / CLAIM
    Radio-->>Station: Cart response
    Station->>Radio: ASSIGN to selected cart
    Radio-->>Cart: ASSIGN
    Cart-->>Radio: ASSIGN_ACK
    Radio-->>Station: Assignment confirmed
    Station-->>Student: Audible confirmation

    Driver->>Cart: Mark On the way
    Cart->>Radio: STATUS: ON_THE_WAY
    Radio-->>Station: Status update
    Driver->>Cart: Mark Arrived at pickup
    Cart->>Radio: STATUS: ARRIVED_AT_PICKUP
    Radio-->>Station: Status update
    Driver->>Cart: Mark journey completed after drop-off
    Cart->>Radio: COMPLETE
    Radio-->>Station: COMPLETE
    Station-->>Cart: COMPLETE_ACK
    Cart->>Cart: Retire request; activate next FIFO item
```

## Sequence caveats

- The sequence is the target design proposal; exact arbitration and message timing remain to be finalized.
- An acknowledgement confirms MCU-level processing, not physical arrival or human awareness.
- Student cancellation is a separate request-state transition and must be correlated to the same request identity.
