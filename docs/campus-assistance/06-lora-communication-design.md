# 06 — LoRa Communication Design

## 1. Communication scope

The agreed communication medium is LoRa only. The initial architecture is a custom raw-LoRa application protocol using the radio transceiver directly. It is not LoRaWAN unless the project is deliberately redesigned around a LoRaWAN gateway/network server.

No Wi-Fi, Bluetooth, cellular, GPS, internet or cloud link is required for the station-to-cart request workflow.

## 2. Identified radio: Ai-Thinker RA-02 / SX1278

The currently identified module is the Ai-Thinker RA-02, commonly built around the Semtech SX1278 transceiver.

**Critical band constraint:** SX1278 is a sub-GHz device commonly used in 433 MHz modules; it is not an 865–867 MHz-capable radio. A firmware frequency constant cannot change the physical RF capability of the chip and matching network. Indian licence-exempt SRD operation commonly discussed around 865–867 MHz therefore requires a radio/module variant designed for that band, not a 433 MHz SX1278 RA-02.

Before any campus transmission:
1. Read the exact module marking and manufacturer datasheet.
2. Confirm the transceiver and RF matching network support the chosen band.
3. Select an antenna designed for that band.
4. Check current Indian WPC/DoT rules for frequency, power/EIRP, bandwidth, duty cycle, channel access and equipment compliance.
5. Record the regulatory source and date in the test report.

The 433 MHz value found in the existing prototype is a bench default only, not legal approval or a final design choice.

## 3. Modem parameters

LoRa modem configuration includes:
- Carrier frequency.
- Spreading factor (SF).
- Signal bandwidth (BW).
- Coding rate (CR).
- Preamble length.
- Sync word / network separation.
- CRC enablement.
- Transmit power.

These settings affect sensitivity, airtime, throughput, interference tolerance and regulatory compliance. Every node in one network must use compatible settings. Do not select final values by guesswork.

Recommended engineering sequence:
1. Confirm the legal band and physical module.
2. Select a conservative initial modem profile supported by the radio and local rules.
3. Calculate approximate packet airtime using the actual payload and modem settings.
4. Measure packet delivery, RSSI, SNR and latency at representative campus locations.
5. Repeat with multiple stations/carts and realistic request/status traffic.
6. Freeze and document the profile only after testing.

## 4. Airtime and capacity

LoRa is a shared, half-duplex radio medium. Longer packets and slower data rates occupy the channel longer. Broadcast requests, acknowledgements, retries and status updates all consume airtime.

For each packet type, record:
- Payload length in bytes.
- SF, BW, CR and preamble.
- Calculated airtime.
- Measured transmit-to-receive latency.
- Retry count and success rate.

A rough network load model is:

`Channel load ≈ Σ(packet rate_i × airtime_i)`

This is only a first-order estimate. It does not include collisions, hidden nodes, regulatory duty-cycle restrictions, retransmission bursts or receiver deafness during transmission. Validate with multi-node tests.

## 5. Proposed application message families

The protocol should be compact and versioned. Exact byte offsets, endianness, sizes and enumerations belong in a normative protocol specification maintained alongside firmware.

Minimum message families:

| Message | Direction | Purpose |
|---|---|---|
| REQUEST | Station → broadcast | New pickup/destination request |
| REQUEST_ACK | Cart → station | Valid request received/queued |
| CLAIM | Cart → station | Cart offers to service request |
| ASSIGN | Station → selected cart / broadcast | Confirms servicing cart |
| ASSIGN_ACK | Cart → station | Confirms assignment |
| STATUS | Cart → station | Received / On the way / Arrived at pickup |
| STATUS_ACK | Station → cart | Acknowledges status event |
| STUDENT_CANCEL | Station → broadcast or assigned cart | Student cancellation |
| CANCEL_ACK | Cart → station | Confirms cancellation processing |
| COMPLETE | Cart → station | Driver confirms destination drop-off |
| COMPLETE_ACK | Station → cart | Confirms completion processing |
| QUEUE_FULL | Cart → station | Cart cannot accept more pending requests |
| HEARTBEAT (optional) | Cart → network | Node liveness / diagnostic signal |

The exact set can be reduced if a single message type with a state field is more efficient. The important requirement is that request identity and state transition are unambiguous.

## 6. Packet identity and duplicate protection

Every request needs an identity that remains unchanged across retries. Recommended tuple:

`(network_id, station_id, request_sequence, generation)`

Every device packet also includes:
- Protocol version.
- Sender device class and ID.
- Message type.
- Request identity where applicable.
- Payload length or fixed-size structure definition.
- Optional flags/reserved fields for future compatible extensions.

Do not rely on a request sequence alone if it can wrap or restart after power loss. Generation/session handling must prevent an old delayed packet from being mistaken for a new request.

## 7. Acknowledgements and retries

- A sender retries the same transaction identity, not a newly generated request.
- A receiver acknowledges duplicate valid packets without re-enqueuing them.
- Retries are bounded by attempt count and transaction deadline.
- Retry intervals must account for packet airtime and expected response delay.
- Simultaneous cart responses can collide. Use a simple contention-reduction delay/slot if required, then test it under load.
- Acknowledgement means the receiving MCU accepted the packet. It does not prove a driver saw it or a cart physically moved.

## 8. Request ownership

Multiple carts may hear the same broadcast. The protocol must ensure that only one cart owns and services a request. The proposed first-winner claim/assignment flow is described in [03 — System Architecture](03-system-architecture.md). Its timing and collision behavior are still open design items.

Never let a cart treat its own transmitted CLAIM as proof that the station assigned it the request.

## 9. RF measurement plan

At a minimum, record:
- Exact module and antenna part/variant.
- Antenna placement and height.
- Frequency and complete modem profile.
- Transmit power setting and legal basis.
- Distance and environment (line of sight, building interior, walls, corridor, outdoor).
- Packet count, successful receptions, missed receptions and retries.
- RSSI and SNR distribution, not only best values.
- Request/status latency.
- Supply voltage during transmit.
- Date, weather/occupancy where relevant, and test setup.

Test station-to-cart and cart-to-station directions separately. LoRa links can be asymmetric due to antenna placement, interference and power supply differences.

## 10. Security boundary

LoRa CRC detects accidental corruption; it is not encryption or authentication. Device IDs are identifiers, not proof of identity. A malicious or misconfigured transmitter could forge a request or status packet if the protocol has no authentication.

For the initial academic prototype:
- Do not transmit student names or sensitive personal data.
- Use non-identifying request IDs and station labels.
- Document the lack of cryptographic authentication.
- Consider a message authentication code and replay protection as a later security enhancement, with key provisioning and memory cost evaluated.
- Do not claim the system is secure merely because it uses LoRa or CRC.

## 11. Regulatory and deployment gate

No field RF deployment is approved by this document. The project team must verify the currently applicable Indian WPC/DoT requirements and equipment conformity for the exact radio variant and settings. A common 433 MHz RA-02 must not be configured to 865–867 MHz in software.
