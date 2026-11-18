# 08 — Safety, Privacy and Accessibility

## 1. Safety boundary

This system coordinates requests for human-driven electric-cart assistance. It is not a vehicle control system.

The firmware must not:
- Drive, steer, accelerate, brake or immobilize the cart.
- Override driver judgment or campus traffic rules.
- Claim that a cart has physically arrived based only on a transmitted status packet.
- Treat loss of radio contact as proof that a cart has stopped.
- Automatically dispatch a second cart when the first cart's physical state is uncertain.

The driver remains responsible for vehicle operation. The institution must define a safe operating procedure, including what happens when a student cancels after the cart has departed.

## 2. Communication uncertainty

A lost packet can leave station and cart with different views of a request. Therefore:
- Critical state changes require acknowledgements.
- Retries reuse the same transaction identity.
- A timeout is shown as **not confirmed** or **communication uncertain**, not success.
- A station must not silently reassign a request if a cart may already be travelling.
- Recovery after reboot or link loss must reconcile the request state or require operator intervention.

A radio ACK confirms processing by the receiving MCU, not that a human saw or acted on the information.

## 3. Accessibility requirements

The target user includes blind and visually impaired students. LCD text is not sufficient as the sole student feedback channel.

Minimum accessibility principles:
- Every important student-facing state has audible feedback.
- Destination selection must be possible without reading the LCD.
- Buttons must be tactilely distinguishable and easy to locate.
- Short/long press behavior must be learnable and consistent.
- Error and cancellation feedback must be distinct from success feedback.
- Beep patterns must not be the only indication of a critical safety condition if the pattern is difficult to distinguish in noise.
- Provide a clearly documented manual fallback, such as contacting campus assistance staff, if the station fails.

## 4. Critical unresolved usability issue: destination selection

A simple buzzer cannot speak a destination name. If a student cycles through a list of destinations, the system needs a nonvisual way to identify the currently highlighted destination.

Options to evaluate:
1. Counted beep patterns associated with a printed/tactile station directory.
2. Distinct tactile labels or a braille/raised-print destination index.
3. A simple sequence where the student knows the order of registered destinations.
4. Staff-assisted destination selection during early prototype trials.

Do not claim the station is fully accessible until intended users validate the actual destination-selection interaction. A future voice module may be considered, but it is not part of the current agreed design.

## 5. Audible pattern evaluation

Measure and evaluate:
- Frequency and duration of each beep.
- Pattern distinguishability.
- Audibility at expected user distance.
- Performance under realistic campus noise.
- Whether patterns are confusing or fatiguing.
- Whether a user can reliably identify request confirmation, cart assignment, arrival, cancellation and error.

Test with blind and visually impaired participants only with informed consent, institutional approval where required, and a safe supervised procedure. Do not infer accessibility from developer preference.

## 6. Driver interface

The cart LCD is intended primarily for the driver. It should show:
- Active request origin.
- Destination.
- Current state.
- Next permitted action.
- Pending queue count.
- Communication uncertainty/error indication.

Two-button controls must avoid accidental skipping of stages. A long press or explicit confirmation may be used for consequential actions. The driver must be able to stop interacting and focus on safe driving; the cart interface must not encourage button operation while the vehicle is moving if that would distract the driver. Define when status updates are made (e.g. stopped at safe points).

## 7. Privacy

- Do not broadcast student names, phone numbers, medical details or disability information.
- Use opaque request IDs and station IDs.
- Avoid persistent logs linking a named student to travel patterns.
- Limit diagnostic logs to operational data.
- Define log retention and access if any records are stored.
- Do not expose device IDs as authentication credentials.

## 8. Security

Raw LoRa broadcasts are observable and can be spoofed unless authentication is added. CRC is not cryptographic protection.

Threats include:
- Forged requests.
- Forged status/completion messages.
- Replay of old requests.
- Duplicate or malicious queue flooding.
- Unauthorized cancellation.

For the prototype, document these limits and test malformed/replayed packets. A later secure design should consider a message authentication code, replay counter/window, key provisioning and key rotation. Do not invent a security guarantee.

## 9. Physical and electrical safety

- Use a protected, fused power source.
- Keep radio supply stable during transmit.
- Secure modules and wires against vibration.
- Protect station enclosures from weather and tampering as required.
- Keep electronics isolated from vehicle traction/control wiring.
- Provide a manual fallback if the electronics fail.
- Do not conduct field trials without institutional approval and driver supervision.

## 10. Risk register

| Hazard / failure | Consequence | Mitigation |
|---|---|---|
| Student cannot identify destination | Wrong destination request | Nonvisual destination coding and user testing |
| Duplicate request due to lost ACK | Duplicate dispatch | Stable request ID and deduplication |
| Two carts claim same request | Conflicting service | Ownership handshake and station arbitration |
| Cart disappears during journey | Uncertain physical status | Mark uncertain; no automatic unsafe redispatch |
| Cancellation arrives late | Driver may continue unaware | Retry, acknowledge, clear driver alert and safe procedure |
| Queue full | Request not serviced | Explicit queue-full response and station feedback |
| Radio band/configuration illegal | Regulatory breach | Verify module, WPC rules and RF settings before transmission |
| Radio supply droop | Intermittent communication/reset | Regulated rail, decoupling and TX measurement |
| Driver distracted by controls | Driving hazard | Use controls only at safe stops; simple display and interaction |
| Buzzer masked by noise | Student misses feedback | Audible testing and manual fallback |
