# Test strategy

Run source-contract checks locally:

```sh
python3 -m unittest discover -s tests -v
```

The GitHub Actions workflow also compiles all three Arduino Uno build variants:

1. Messaging firmware
2. Campus station role
3. Campus cart role

Source-contract tests are regression guards for required configuration and protocol hooks. They do **not** simulate radio behaviour or replace hardware-in-the-loop testing.

## Required bench and field test matrix

- Two-node and three-node messaging; dynamic peer discovery and expiry.
- 128-byte message, empty message, 129-byte message, CR/LF input, non-ASCII UTF-8 byte-length edge cases.
- Lost ACK, delayed ACK, duplicate packet, corrupted packet, and simultaneous ACK collision.
- Power cycle and EEPROM ID persistence; duplicate-ID commissioning check.
- Multiple stations broadcasting concurrently; FIFO order at each cart.
- Two carts claiming one request; lost claim, lost assignment, repeated assignment, duplicate assignment ACK.
- Student cancellation before and after cart acceptance; cancellation packet loss and retries.
- Cart-side cancellation, completion, cart power loss, heartbeat loss and reassignment.
- Queue capacity 10 and queue-full response.
- Request expiration and stale-generation rejection.
- Button bounce, held button, accidental press, buzzer/LED status, LCD readability.
- RF range, packet delivery, airtime, interference and legal operating configuration on the actual campus.

Do not connect this prototype to vehicle drive/brake controls. Validate the request/dispatch workflow with stationary test equipment before testing with moving carts.
