# System Architecture Diagram

```mermaid
flowchart TB
    S1["Station ST001<br/>Library"]
    S2["Station ST002<br/>Canteen"]
    SN["Additional stations<br/>firmware-provisioned"]
    RF(("Shared LoRa<br/>broadcast medium"))
    C1["Cart CRT001<br/>local FIFO queue"]
    C2["Cart CRT002<br/>local FIFO queue"]
    CN["Additional carts<br/>firmware-provisioned"]
    S1 <--> RF
    S2 <--> RF
    SN <--> RF
    RF <--> C1
    RF <--> C2
    RF <--> CN
```

## Notes

- Diagram is logical, not a coverage map.
- Every node uses compatible RF/modem settings and network/protocol configuration.
- Requests are broadcast; a claim/assignment exchange is required to prevent duplicate cart service.
- FIFO ordering is local to each cart queue, not one globally synchronized campus queue.
- The system does not use a cloud server or non-LoRa communication path.
