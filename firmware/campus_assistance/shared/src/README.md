# Shared implementation boundary

Portable implementations belong in focused translation units and must remain
C++11-compatible without Arduino or ESP-IDF dependencies. Domain headers include
portable value types directly; hardware, runtime, persistence and repository ports
are separated under `include/ca/ports/`.

`config.cpp` contains the default provisioned-configuration validator. `types.cpp`
contains shared wrap-safe time/sequence helpers and request/journey-state predicates.
Implemented portable components include station request lifecycle, repository-backed
claim coordination, cart claim ownership, authoritative event reconciliation, station
request enqueueing, destination lookup, and a bounded CRC-protected binary frame
codec. Full message payload serialization, retry scheduling/endpoint orchestration,
application event loops, physical persistence, radio drivers and MCU adapters remain
future implementation work.

The default validator checks structural validity and the selected RA-02/SX1278
parameter limits. It does not establish WPC authorization, board-profile compatibility,
RF performance or hardware correctness.
