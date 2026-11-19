# Verification plan — not yet implemented

Add host-side tests before hardware integration.

Fakes: radio (drop/duplicate/reorder/delay/corrupt), clock (expiry and rollover), persistence (reboot/torn write/failure), buttons (bounce/short/long), display and buzzer.

Required suites:
1. Shared type semantics: serial-number ordering, half-range ambiguity, millis rollover,
   terminal-state classification and journey-status validation.
2. Full RequestId validation/equality/hash and generation rollover.
3. Pending FIFO, deduplication, capacity, local-only clear and exact removal.
4. Driver state: boot NOT_ACCEPTING, no claim while disabled, explicit enable, completion forces disabled.
5. Competing claims: exactly one station winner; CLAIM alone never activates.
6. Missed/duplicate/delayed ASSIGN, CANCEL and EXPIRED reconciliation.
7. 600000 ms default expiry; CLAIM/expiry boundary; assigned requests not pending-expired.
8. Journey progression, stale status and ACK semantics.
9. Codec golden vectors, malformed frames and Uno/ESP32 compatibility.
10. Uno and selected ESP32 memory/stack/time budgets.
11. RF, power, thermal, accessibility and regulatory validation.

A protocol ACK is not proof of physical completion or human awareness.
