# Verification plan — not yet implemented

Add host-side tests before hardware integration.

Fakes: radio (drop/duplicate/reorder/delay/corrupt), clock (expiry and rollover), persistence (reboot/torn write/failure), buttons (bounce/short/long), display and buzzer.

Required suites:
1. Full RequestId validation/equality and generation rollover.
2. Pending FIFO, deduplication, capacity, local-only clear and exact removal.
3. Driver state: boot NOT_ACCEPTING, no claim while disabled, explicit enable, completion forces disabled.
4. Competing claims: exactly one station winner; CLAIM alone never activates.
5. Missed/duplicate/delayed ASSIGN, CANCEL and EXPIRED reconciliation.
6. 600000 ms default expiry; CLAIM/expiry boundary; assigned requests not pending-expired.
7. Journey progression, stale status and ACK semantics.
8. Codec golden vectors, malformed frames and Uno/ESP32 compatibility.
9. Uno and selected ESP32 memory/stack/time budgets.
10. RF, power, thermal, accessibility and regulatory validation.

A protocol ACK is not proof of physical completion or human awareness.
