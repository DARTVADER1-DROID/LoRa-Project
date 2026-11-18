# 05 — Hardware Design

## 1. Hardware baseline

Agreed/identified components:
- Arduino Uno and ESP32 as target MCU families, with dedicated firmware builds.
- Ai-Thinker RA-02 LoRa module based on Semtech SX1278 as the currently identified radio.
- 16×2 character LCD.
- Maximum two physical buttons per station and per cart.
- Buzzer for simple audible feedback.
- Fixed station enclosure and cart-mounted enclosure.
- Antenna appropriate to the exact radio frequency.

The final BOM, power architecture, LCD interface voltage, button part numbers, buzzer current and enclosure design remain to be measured/selected.

## 2. Node block diagrams

### Station

```
2 x buttons ─┐
16x2 LCD  ───┼──> MCU (Uno or ESP32) <──SPI──> LoRa RA-02
Buzzer    ───┘          |
                   Power supply
```

### Cart

```
2 x buttons ─┐
16x2 LCD  ───┼──> MCU (Uno or ESP32) <──SPI──> LoRa RA-02
Buzzer    ───┘          |
                   Cart auxiliary supply
```

The LoRa node is only a communication and user-interface device. It must not be connected to steering, braking, throttle or traction-control circuits.

## 3. MCU variants

### Arduino Uno

- ATmega328P, 5 V logic on the common Uno R3.
- Limited SRAM (2 KB on the classic ATmega328P Uno) requires compact packet structures, static buffers and small bounded queues.
- RA-02 is a 3.3 V device; Uno SPI outputs must be translated to safe 3.3 V logic.
- I2C pins on classic Uno: SDA=A4, SCL=A5.
- Use a separate regulated 3.3 V supply with sufficient transient current capability for the radio.

### ESP32

- 3.3 V logic on common ESP32 development boards; exact board variant must be recorded.
- Pin assignments depend on the specific ESP32 module/dev board. Avoid assuming a universal pin map.
- Many ESP32 boards have more RAM and may support larger queues, but the protocol and functional behavior must remain compatible with Uno nodes.
- Do not apply 5 V signals to ESP32 GPIO.
- Verify the board's 3.3 V regulator capability and the current available to the RA-02 during transmit.

Dedicated firmware builds should use a board-specific configuration header for SPI pins, I2C pins, button pins, buzzer pin, radio frequency and MCU-specific storage implementation.

## 4. RA-02 / SX1278 electrical interface

The RA-02 is a 3.3 V radio module. Do not power it from an MCU board's 3.3 V output unless the board regulator's transient capacity and thermal margin have been verified under radio transmission.

Typical SPI signals:
- SCK: MCU SPI clock.
- MISO: radio-to-MCU data.
- MOSI: MCU-to-radio data.
- NSS/CS: chip select.
- RESET: radio reset.
- DIO0: radio interrupt/status signal, if used by the library/configuration.
- VCC: regulated 3.3 V.
- GND: common ground.

For Uno:
- Level-shift 5 V outputs (SCK, MOSI, NSS, RESET) to 3.3 V.
- Verify MISO logic compatibility.
- Keep SPI wires short and provide local decoupling.
- Use a suitable level translator/buffer for SPI rather than assuming a resistor divider is adequate at the selected clock rate.

For ESP32:
- Direct 3.3 V logic is generally compatible, subject to exact board and module specifications.
- Verify boot-strapping pin restrictions before choosing GPIOs.

## 5. LCD interface

The agreed display is a 16×2 character LCD. An I2C backpack is a practical option to reduce GPIO use, but the backpack voltage and logic levels must be checked.

- Uno I2C: A4 SDA and A5 SCL.
- ESP32 I2C pins: explicitly configured for the selected board.
- Common I2C address 0x27 is not guaranteed; scan/verify the actual module.
- If an I2C backpack is powered at 5 V, check whether its SDA/SCL pull-ups expose ESP32 GPIO to 5 V. Use a level shifter or 3.3 V-compatible arrangement as required.
- The LCD is primarily useful to the driver and maintainers. The blind student interface cannot depend on visual reading.

## 6. Buttons

Maximum count:
- Station: 2.
- Cart: 2.

Recommended electrical arrangement:
- Momentary normally-open switches.
- Internal pull-up where supported; active-low input.
- Software debounce, initially around 30–50 ms, then validate with real switches.
- Distinguish short press and long press with a clear timing policy.
- Avoid blocking delay-based input handling.
- Use tactilely distinguishable caps or spacing so the controls can be identified by touch.

No third physical button should be added to work around an unclear state machine. Rework the menu/state-dependent interaction instead.

## 7. Buzzer

A simple active buzzer is easiest to control; a passive buzzer allows tones but may need a transistor driver depending on current. Select the part from its datasheet.

- Do not exceed MCU GPIO current ratings.
- Use a transistor/MOSFET driver if the buzzer current exceeds safe GPIO capability.
- Define non-overlapping patterns for key states.
- Avoid continuous tones that mask environmental sounds or create nuisance.
- Test audibility in representative indoor/outdoor campus noise.

## 8. Power architecture

The power source has not yet been fixed. Candidate sources include a regulated station supply and an appropriately protected auxiliary supply on the cart. Do not assume the cart traction battery can be connected directly to the electronics.

For each node, determine:
1. Input voltage range and transients.
2. Required regulated rails (MCU board input, 3.3 V radio, LCD/buzzer rail).
3. Maximum simultaneous current, including radio transmit peaks and LCD backlight.
4. Regulator efficiency and thermal dissipation.
5. Reverse-polarity, overcurrent and transient protection.
6. Fuse and wiring requirements.
7. Grounding and enclosure/EMI considerations.
8. Battery capacity and runtime if battery powered.

Power calculations must use measured current in idle, receive, transmit, LCD backlight and buzzer states. Do not publish battery-life estimates until those values are measured.

## 9. Mechanical and installation considerations

- Station should be reachable from a wheelchair and by a standing user; mounting height and reach envelope require accessibility review.
- Button labels should be tactile and distinguishable.
- LCD should be angled for driver visibility and protected from weather/glare as applicable.
- Antenna should be mounted away from large metal obstructions and high-current wiring.
- Cart enclosure must be mechanically secured and protected from vibration, dust and moisture appropriate to the campus environment.
- Keep the antenna clear of the driver's body and conductive enclosure surfaces.
- Provide service access without exposing students to wiring or sharp edges.

## 10. Preliminary pin-map policy

Do not publish one universal pin map for both MCU families. Maintain:
- `config_uno.h`
- `config_esp32_<board>.h`

Each file must list every GPIO, SPI/I2C pin, active polarity, boot-strapping restriction, peripheral voltage and connector label. Pin allocation is intentionally an open item until the exact ESP32 board and physical modules are selected.

## 11. Electrical safety checklist

- [ ] Verify exact MCU board and radio module markings.
- [ ] Verify radio supply rail at idle/RX/TX with an oscilloscope or suitable instrument.
- [ ] Verify Uno-to-radio level shifting.
- [ ] Verify LCD I2C pull-up voltage.
- [ ] Verify buzzer drive current and transistor stage if needed.
- [ ] Verify no connection to vehicle control wiring.
- [ ] Secure wiring, fuse supply and strain-relieve connectors.
- [ ] Perform thermal checks during repeated radio transmissions.
- [ ] Do not transmit without a correctly matched antenna or suitable RF load.
