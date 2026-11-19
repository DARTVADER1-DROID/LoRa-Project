# Shared implementation boundary

Portable implementations belong in focused translation units and must remain
C++11-compatible without Arduino or ESP-IDF dependencies.

`config.cpp` contains the default provisioned-configuration validator. `types.cpp`
contains shared wrap-safe time/sequence helpers and request/journey-state predicates.
Domain services,
protocol processing, persistence and platform adapters remain future implementation
work.

The default validator checks structural validity and the selected RA-02/SX1278
parameter limits. It does not establish WPC authorization, board-profile compatibility,
RF performance or hardware correctness.
