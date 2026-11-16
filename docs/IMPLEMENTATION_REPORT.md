# LoRa Project — Implementation Report

**Status:** Code-reviewed implementation report. Hardware and RF behavior are not certified by this document.

## 1. Executive summary

The repository contains two independent Arduino LoRa firmware prototypes and a desktop serial-port discovery application.

1. **Group messaging:** shared-group text messaging with HELLO peer discovery, message/session identifiers, per-recipient acknowledgements, bounded retries, duplicate suppression, and delivery outcome reporting.
2. **Campus mobility assistance:** fixed-station/electric-cart workflow with request broadcast, cart queues, claim/assignment handshake, cancellation/completion, heartbeat monitoring, and conservative offline-recovery lock.
3. **Desktop application:** Rust + Tauri 2 app. On the current main branch it discovers and displays operating-system serial ports. Port discovery is not a firmware handshake or proof of radio health.

These are separate application modes, use different packet formats/network IDs, and are not interoperable.

## 2. Repository layout

| Path | Responsibility |
|---|---|
| firmware/messaging/messaging.ino | Group messaging firmware |
| firmware/campus_assistance/campus_assistance.ino | Campus station/cart firmware; role selected at compile time |
| messaging-app/ | Rust/Tauri desktop serial-port discovery app |
| docs/ | Hardware, safety, protocol, implementation documentation |
| tests/ | Host-side protocol and reliability tests |
| .github/workflows/ | Arduino compilation and source-contract CI |

## 3. Common hardware platform

Initial target: Arduino Uno R3 / ATmega328P and an SX127x-family LoRa module using the Sandeep Mistry Arduino LoRa library.

### 3.1 Radio-to-Uno wiring

| Radio signal | Uno pin | Notes |
|---|---:|---|
| SCK | D13 | SPI clock |
| MISO | D12 | Radio-to-MCU data |
| MOSI | D11 | MCU-to-radio data |
| NSS / CS | D10 | Chip select |
| RESET | D9 | Radio reset |
| DIO0 | D2 | Radio status/interrupt |
| VCC | External regulated 3.3 V | Must support TX current and transients |
| GND | GND | Common reference |

Uno logic is 5 V; the radio module is a 3.3 V device. Level-shift Uno outputs to safe 3.3 V levels for SCK, MOSI, NSS and RESET. A suitable 3.3 V buffer/translator is preferred for SPI. Do not assume the Uno 3.3 V output can supply transmit-current peaks. Decouple the radio locally and use an antenna matched to the exact RF module.

### 3.2 RF configuration warning

Both sketches currently define 433 MHz as a bench default. This is not a statement of legal operating frequency or field approval. Semtech specifies SX1278 for 137–525 MHz; it cannot operate at 865–867 MHz simply by changing a software constant. Select the exact module/front-end and antenna for the intended jurisdiction. Verify current WPC requirements, permitted channel, bandwidth, power/EIRP and duty-cycle before transmitting. See the Semtech SX1278 product information: https://www.semtech.com/products/wireless-rf/lora-connect/sx1278

## 4. Firmware A — group messaging

### 4.1 Purpose and operating model

This is a shared RF group protocol, not LoRaWAN. Devices broadcast HELLO packets and maintain a bounded table of recently heard peers. A sender snapshots currently active peers when a text message is queued, then waits for acknowledgements from that snapshot.

Current configuration:
- Network ID: 1001
- Protocol version: 1
- User label: DEVICE_USER
- Maximum message: 128 bytes
- Peer table: 8 entries
- Peer expiry: 90 seconds
- HELLO interval: 3 seconds if no peers are active; 20 seconds otherwise; 0–1000 ms random jitter
- Serial console: 9600 baud

### 4.2 Radio frame

The packed RadioPacket is statically asserted to be 172 bytes. Fields are: magic 0x4C52; protocol version; network ID; packet type; sender ID; persistent boot/session epoch; message ID; ACK correlation tuple (original sender ID, boot epoch and message ID); payload length; 16-byte user name; and 129-byte text storage.

Packet types are HELLO=1, TEXT=2 and ACK=3. The receiver rejects frames with the wrong size, magic, version, network ID, type, or invalid text/ACK fields. Radio CRC is enabled. Changing field order, type, packing, or capacity changes the wire format; increment the protocol version and update every node together.

### 4.3 Identity and duplicate suppression

On first boot, a 16-bit candidate device ID is generated using analog A0, micros() and millis(), then stored in EEPROM with a magic marker. A 32-bit boot/session epoch is incremented and persisted at startup. The ID + epoch pair distinguishes device sessions for message correlation.

This is not secure provisioning and does not guarantee uniqueness. Analog-seeded pseudo-randomness is not cryptographic. Commissioning must detect and resolve duplicate IDs. The ID is not a certified hardware serial number.

A 12-entry circular cache stores sender ID, sender epoch and message ID. Duplicate DATA is not printed again, but an ACK is still scheduled so a sender can recover from a lost ACK.

### 4.4 Delivery and retries

1. Serial input is collected until newline; CR is ignored.
2. Empty text, NUL-containing lines and lines longer than 128 bytes are rejected.
3. A second message is rejected while a prior transaction is active.
4. The sender snapshots the active peer IDs and boot epochs.
5. Up to four total TEXT transmissions are attempted: initial send plus three retries.
6. Each attempt waits up to 4200 ms for ACKs. Exponential backoff starts at 150 ms, caps at 1200 ms, and includes random jitter.
7. A 22-second maximum transaction lifetime bounds the operation.
8. ACKs count only when message ID, destination identity/session and expected sender identity/session match. Duplicate and unexpected ACKs are ignored.

Final serial outcomes:
- CONFIRMED_TO_n: every peer in the send-time snapshot acknowledged.
- PARTIALLY_CONFIRMED: some, but not all, expected peers acknowledged.
- UNCONFIRMED: peers were expected but none acknowledged.
- NO_KNOWN_RECIPIENTS: one best-effort broadcast was attempted; no delivery confirmation was possible.

This does not prove every physical receiver in range is known. The expected set is only the peer table at send time.

### 4.5 ACK contention handling

Receivers use a bounded four-entry ACK queue. Duplicate ACK work for the same message/target/session is coalesced. ACK delay uses a deterministic slot derived from message and node identity, 50 ms minimum turnaround, 450 ms slot spacing, and 0–50 ms jitter. A failed ACK transmission is retried after 250 ms, up to four attempts. Queue overflow is logged; sender retries may recover.

This is a lightweight contention-reduction heuristic, not a formal MAC protocol. Test with the expected node count and traffic pattern.

### 4.6 Modem profile

The messaging sketch configures 433 MHz bench default, SF7, 125 kHz bandwidth, coding rate 4/5, preamble length 8, and CRC enabled. Both ends must use compatible settings. These values are not validated for range, throughput, coexistence, or regulatory compliance.

## 5. Firmware B — campus mobility assistance

### 5.1 Roles and configuration

One sketch is compiled as either DEVICE_ROLE_STATION or DEVICE_ROLE_CART. The default role is station; define DEVICE_ROLE=2 for the cart build. Station and each cart require unique, deliberately provisioned DEVICE_ID values. The current source default DEVICE_ID=1 is a template and must not be reused across deployed nodes.

Campus network ID is 2001. Default station location is LIBRARY. Device identity and location are compile-time settings.

### 5.2 Campus packet

The packed CampusPacket is 30 bytes: magic 0xCA51, network ID, packet type, sender ID, station ID, cart ID, request ID, 8-bit generation and 16-byte location. It has 14 packet types: REQUEST, REQUEST_ACK, CLAIM, ASSIGN, STUDENT_CANCEL, CART_CANCEL, COMPLETE, HEARTBEAT, EXPIRE, ASSIGN_ACK, QUEUE_FULL, CANCEL_ACK, CART_CANCEL_ACK and COMPLETE_ACK.

Radio CRC is enabled. The receiver checks exact packet size, magic and network ID. This is a custom raw-LoRa application protocol, not LoRaWAN, and the campus frame has no explicit protocol-version field. All participating nodes must run compatible firmware.

### 5.3 Station implementation

- Short button press while idle creates a request.
- Holding the station button for at least 3 seconds during an active request cancels it.
- REQUEST is broadcast every 5 seconds.
- An unassigned request expires after 10 minutes.
- Distinct cart claims are collected for 900 ms.
- The station chooses a claimant using a rotating-ID preference based on the last winning cart, then broadcasts ASSIGN.
- An unconfirmed assignment is retransmitted every 1.5 seconds.
- The assignment is confirmed only after matching ASSIGN_ACK.
- Matching cart heartbeat is expected every 5 seconds.
- After 30 seconds without a matching heartbeat, the station enters an offline-uncertain lock.
- A returning heartbeat does not clear the lock. An operator must physically inspect the cart and then hold the station button for 5 seconds before redispatch is allowed.
- A four-entry terminal cache allows repeated cart cancel/complete messages to be acknowledged.

### 5.4 Cart implementation

- Accepts REQUEST into a bounded queue of up to 10 requests.
- Deduplicates by station/request identity and tracks request generation.
- Sends delayed REQUEST_ACK; sends QUEUE_FULL when the queue is full.
- Requires an explicit accept-button press to claim the queue head.
- Sends CLAIM and waits for station ASSIGN; a claim alone does not activate a journey.
- Displays assignment/queue state on a 16x2 I2C LCD.
- Sends heartbeat every 5 seconds.
- Uses a 3-second cancel-button hold for cart cancellation.
- Sends COMPLETE on the completion button.
- Retries terminal cancel/complete messages every 1.5 seconds, up to four transmissions, while awaiting station acknowledgement.
- Uses a bounded four-entry reply queue with randomized 60–649 ms delay.
- Maintains a 12-entry tombstone cache to reject stale request generations.

Generation comparison uses modular 8-bit sequence arithmetic. Tombstones are RAM-only and are lost on reboot.

### 5.5 Campus pin map

**Radio interface, both roles:** SCK D13, MISO D12, MOSI D11, NSS D10, RESET D9, DIO0 D2; external regulated 3.3 V and common GND.

**Station role**

| Function | Uno pin |
|---|---:|
| Request/cancel button | D3 |
| RGB LED red | D4 |
| RGB LED green | D5 |
| RGB LED blue | D6 |
| Buzzer | D7 |

**Cart role**

| Function | Uno pin |
|---|---:|
| Accept button | D3 |
| Cancel button | D4 |
| Complete button | D5 |
| Status LED | D6 |
| Buzzer | D7 |
| LCD SDA | A4 |
| LCD SCL | A5 |

Buttons use INPUT_PULLUP and are active-low; software debounce is 40 ms. LCD I2C address defaults to 0x27. Station and cart roles are separate builds; the shared pin numbers have role-specific meanings.

## 6. Desktop application

The messaging-app project is a Rust + Tauri 2 app with HTML/CSS/JavaScript frontend. On current main it discovers OS serial ports at startup, refreshes periodically and on demand, and displays port name, transport type and USB metadata when available.

A serial-port entry does not prove the device is the expected Arduino, firmware is running, radio is connected, or RF communication works. Explicit serial connection management and firmware handshake are separate work on feature/serial-connect-status; do not describe those as shipped on main.

## 7. Build and verification

- Install Arduino IDE or Arduino CLI.
- Install the Sandeep Mistry LoRa library.
- For the campus cart build, install a compatible LiquidCrystal_I2C library.
- Select Arduino Uno R3 / ATmega328P.
- Compile the messaging sketch separately.
- Compile campus station and cart roles separately; define DEVICE_ROLE=2 for cart.
- Build the desktop app from messaging-app using its project README.
- Run host tests with: python3 -m unittest discover -s tests -v
- Review GitHub Actions for Arduino compile and source-contract test results.

Compilation and host-side tests do not establish RF range, packet delivery, supply integrity, legal compliance, accessibility, or field safety.

## 8. Security, safety and reliability boundaries

- Network IDs and CRC are used, but there is no sender authentication, encryption, cryptographic replay protection, or secure key provisioning.
- CRC detects accidental corruption; it does not authenticate a sender.
- Messaging device ID generation is not collision-proof.
- Campus firmware is an assistance workflow only. It must never drive, steer, brake, or immobilize a cart.
- Missing heartbeat means communication is uncertain; it does not prove the cart stopped or a person is safe.
- Accessibility testing must cover audible and visual indications. Do not rely on LED colour alone.
- Measure radio supply current, thermal behavior, antenna matching, range, packet error rate, coexistence and battery behavior.
- Never transmit without an appropriate antenna or RF load.

## 9. Engineering work still required

1. Verify exact radio module and regional RF configuration before field use.
2. Replace analog-seeded identity with controlled commissioning and duplicate-ID detection.
3. Add authenticated frames, replay protection and key lifecycle design before sensitive deployment.
4. Strengthen semantic validation of every campus packet field.
5. Decide which station/cart state must survive reset; queues, tombstones and assignment state are currently volatile.
6. Test simultaneous claims, lost ACKs, duplicates, queue overflow, stale generations, reboot mid-request and RF outages.
7. Measure 3.3 V transient response, logic levels, antenna behavior and RF performance.
8. Implement explicit serial open/close, firmware identity handshake and structured telemetry in the desktop app.
9. Complete supervised campus trials, accessibility review, risk assessment and operational fallback before deployment.

## 10. Source of truth

This report was prepared from the repository README, hardware/safety notes and both firmware sketches. Statements about implemented behavior refer to code paths present in those files. Hardware behavior, RF performance, compile success and field suitability remain unverified without separate test evidence.
