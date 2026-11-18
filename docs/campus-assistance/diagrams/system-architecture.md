# System Architecture Diagram

```mermaid
flowchart TB
    S1["Station ST001<br/>Library<br/>Request authority + 10 min expiry"]
    S2["Station ST002<br/>Canteen"]
    SN["Additional stations<br/>firmware-provisioned"]
    RF(("Shared LoRa<br/>broadcast medium"))
    C1["Cart CRT001<br/>driver mode + FIFO pending list<br/>0/1 active assignment"]
    C2["Cart CRT002<br/>driver mode + FIFO pending list<br/>0/1 active assignment"]
    CN["Additional carts<br/>firmware-provisioned"]
    S1 <--> RF
    S2 <--> RF
    SN <--> RF
    RF <--> C1
    RF <--> C2
    RF <--> CN
    C1 -. "CLAIM" .-> RF
    C2 -. "CLAIM" .-> RF
    RF -. "ASSIGN / EXPIRED / CANCEL" .-> C1
    RF -. "ASSIGN / EXPIRED / CANCEL" .-> C2
```

## Operating rules

- All carts continuously listen, including when the driver is not accepting requests.
- Each cart maintains a local FIFO list of unaccepted/unassigned requests.
- Driver acceptance must be enabled before a cart can claim; each cart may hold at most one active assignment.
- The originating station commits one winning cart and announces the assignment. Other carts remove that request from pending when they receive the announcement.
- Disabling acceptance clears that cart's current local pending list only; it does not cancel the station-owned request.
- After journey completion, the driver must manually re-enable acceptance.
- Unaccepted requests expire after 10 minutes by default; the originating station is authoritative.
- FIFO ordering is local to each cart, not one globally synchronized campus queue.
- Diagram is logical, not a coverage map. No cloud server or non-LoRa communication path is used.
