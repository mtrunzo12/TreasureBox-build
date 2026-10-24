# Treasure Box battery and sleep-power review

Reviewed 2026-10-16 against the unchanged v8.4 firmware and the local main-board KiCad schematic/netlist. The user reports four Amazon Basics alkaline AAs lasting about a week, with lights turning off after the expected five minutes. No current, voltage, fuse, or wake-frequency measurements were taken on the physical box during this review.

## v8.5 implementation update

The subsequent v8.5 release implements conditional tilt-only wake for a disarmed, closed, commanded-locked box. Armed and pending-closure states retain lid wake using only the internal reed pull-up. Sensor configuration is restored before normal lid handling after wake or canceled sleep entry. See [QUALITY_REPORT_v8.5.md](QUALITY_REPORT_v8.5.md) for build/upload and test results. BOD disable, wake filtering, and servo/USB power switching remain recommendations; total current has not been measured. The original v8.4 findings below are retained as the baseline.

## Finding

The code requests genuine MCU power-down. It does not turn off the whole board. The powered servo, battery-fed CH340C USB interface, and sensor pull-ups remain possible battery loads. The sensor pull-up drain is directly calculable; the servo and USB chip's actual sleep-state currents remain unmeasured. A week of battery life is therefore not, by itself, proof that the MCU failed to sleep.

For scale, an illustrative 2,000 mAh usable pack capacity consumed in seven days implies about **12 mA average current**. Four cells in series increase voltage, not amp-hour capacity. This is an example budget, not an Amazon Basics capacity claim: usable capacity depends on load, age, and the voltage at which the box stops working.

## What the code already does

`treasure_box/treasure_box.ino:977`, `maybeSleep()`:

- Waits for 300,000 ms of inactivity, with no active music, countdown, held input or setup.
- Turns off all three LED outputs and stops Timer2 audio with the speaker transistor drive LOW.
- Disables the watchdog and ADC, then executes `SLEEP_MODE_PWR_DOWN` with atomic entry checks.
- Enables tilt D3/PCINT19 and lid A3/PCINT11 wake on either edge. There is no periodic watchdog wake scheduled.
- Restores peripherals and starts another five-minute awake period after any wake.

`stopLatchDrive()` already detaches the servo and leaves D10 LOW after movement. It stops signal pulses, but J5 still supplies the servo from the battery rail.

The user's lights-off observation is consistent with reaching the sleep path; it does not prove the board remains asleep continuously. Any tilt contact chatter or lid edge wakes it, and each wake grants another five awake minutes. Logging the wake cause and counting rejected/accepted wake events would distinguish sustained sleep from repeated wake-ups.

## Where current can still flow

Approximate resistor calculations below use a 5.0 V logic rail. Actual rail voltage should be measured.

| Load | Verified connection or behavior | Assessment |
|---|---|---|
| Gripper servo | J5 pin 2 is on `SERVO_V`, after the main switch, before the logic diodes. No firmware-controlled supply switch exists. | Measure first. Detaching PWM cannot guarantee zero standby current. The exact Proton's no-signal standby current is not published. Its specified 80–90 mA no-load current is a running specification, not a standby estimate. |
| CH340C USB interface, U3 | Pin 16 is connected to the battery-fed `+5V` rail, not just USB VBUS. | WCH lists 7 mA typical operating current and 0.05 mA typical USB-suspend current at 5 V. It supports automatic USB suspend, so do not assume it consumes 7 mA with USB absent. Measure its actual state/current. MCU sleep or `Serial.end()` does not power this separate chip off. |
| Lid reed external pull-up | A5 remains HIGH through R6, 10 kOhm, to A3; a closed reed connects A3 to ground. | About **0.50 mA** continuously with the lid shut, or 84 mAh/week. |
| Lid reed internal pull-up | A3 also remains `INPUT_PULLUP`. | Another roughly **0.10–0.25 mA** with the reed closed, using the MCU's specified 20–50 kOhm range. |
| Tilt internal pull-up | D3 is `INPUT_PULLUP`. | Roughly **0.10–0.25 mA** while the tilt contact rests closed; much less with the contact open. |
| MCU brown-out detector | No `sleep_bod_disable()` call; actual device fuses not read in this review. | If BOD is enabled by the fuses, it stays enabled in sleep. The local Uno profile specifies extended fuse 0xFD, but that does not prove this chip's programmed fuses. This is a secondary optimization, not a convincing explanation for a multi-milliamp drain alone. |

Component specifications: [goBILDA Proton Torque 2002-0180-0002](https://www.gobilda.com/proton-servo-steel-gears-180-rotation-torque-ratio/); [WCH CH340 datasheet, manufacturer document mirrored by LCDWiki, §§5.1, 6.2 and 7.7](https://www.lcdwiki.com/res/PublicFile/CH340C_en.pdf); [Microchip ATmega328P datasheet, §§9.10–9.11 and DC characteristics](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf).

The lid plus closed tilt pull-ups total approximately **0.70–1.00 mA**, excluding the MCU, servo and USB chip. That matters, but normally would not explain consuming an illustrative 2,000 mAh in a week by itself. An awake USB interface, servo standby draw, repeated wake-ups, or loss of usable voltage could account for the remainder; none is yet a measured root cause.

## Recommended pathways, in order

### 1. Measure the external loads before changing the sleep policy

Use a battery-only measurement, USB disconnected. Measure total current immediately after power-on and after at least six quiet minutes with the lid shut. A current logger is useful for catching brief wake-ups. For a multimeter, insert it **in series** with the battery lead, using a fused range that tolerates startup/servo current; never place a meter in current mode directly across the batteries. The servo is rated up to 1 A stalled at 6 V, so a small mA-range fuse may not survive startup motion.

Repeat with all three servo leads disconnected at J5, disconnecting power before changing wiring. Leave the lid sensing connected and let the controller finish its normal startup/idle sequence. The difference isolates the servo branch's contribution. Do not interpret this test as a functioning lock.

If substantial drain remains, measure/isolate U3's supply through a deliberate rework or instrumented test point. Simply unplugging USB does not isolate U3 because it is battery-fed. Avoid lifting pins or cutting traces until the rework is planned, including UART/DTR back-power paths.

Also record each cell, pack voltage, and the logic rail at rest and during a claw movement. The servo specifies 4.8–6.0 V, while the logic rail is fed through two 1N4007 diodes. This is not a regulated supply. The box can stop functioning under load before all chemical capacity is exhausted; a weak cell or rail sag must be distinguished from a high sleep current.

### 2. Firmware-only reduction on the existing PCB

These were the proposed changes at review time; the internal-only reed pull-up option is now implemented in v8.5 when lid wake is needed. The duplicate-pull-up alternative and other items remain proposals:

- **Remove the duplicate reed pull-up.** Lowest-risk first step: keep A5 HIGH and R6 active, but disable A3's internal pull-up during sleep. The existing strong 10 kOhm pull-up still supports lid wake; saving is roughly 0.10–0.25 mA with the lid closed.
- **For a larger reed saving, use the internal pull-up alone during sleep.** Make A5 an input with its pull-up disabled, leaving R6's supply end high impedance, while keeping A3 `INPUT_PULLUP`. This removes about 0.50 mA at 5 V. Restore A5 HIGH on wake and test for false wakes on the actual wires. This is an alternative to the preceding configuration; never disable both pull-ups. **Do not drive A5 LOW**: that pulls A3 toward ground through R6 and can prevent lid-open wake.
- **Disable BOD only during power-down**, using the AVR timed sleep sequence; retain brown-out protection while awake. Confirm the actual fuse configuration and low-battery wake behavior. [Microchip describes automatic BOD restoration on wake](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/8-bit-avr/structure/bod/).
- **Filter nuisance tilt wakes.** Record which pin woke the MCU; validate a brief tilt event and return to sleep promptly if there is no lid change or deliberate interaction. Preserve immediate handling of an armed lid opening. Only give the full five-minute awake period after accepted activity. Verify that filtering still notices short deliberate tilts, not merely a final switch level.
- **Add temporary sleep diagnostics**: wake counts by cause, last sleep inhibitor, and battery-only current traces. Do not periodically wake just to print status. Shortening the five-minute timer reduces awake overhead, but cannot remove persistent servo/USB draw.

Blanket peripheral shutdown is not the main missing feature: the MCU's synchronous clocks already stop in power-down, and the ADC/watchdog are explicitly disabled. Preserve Timer0 timing, Timer1 servo control and Timer2 audio on wake. Avoid globally disabling digital inputs used for tilt/lid wake.

### 3. Switch the servo's supply if measurement shows meaningful standby draw

Add a high-side load switch or properly driven MOSFET stage to the **servo branch only**, rated for fresh-pack voltage and startup/stall current with margin. The shared `SERVO_V` node also feeds the logic diodes, so switching that entire node would shut off the MCU and defeat wake-up.

Keep servo ground common. Hold its signal LOW or appropriately high impedance while unpowered to prevent signal-pin back-powering. Switch supply on before a commanded move, allow measured startup settling, send the existing endpoint pulses, finish the last pulse cleanly, then switch supply off. Check that removing power does not release or back-drive the mechanical latch; the three-wire servo provides no position feedback.

All normal GPIOs are already assigned. One possible revision is to move R6's supply to a fixed, suitably weak pull-up and reuse A5 for the switch enable. That needs an explicit schematic/PCB or daughterboard design; A5 must never supply servo current directly. Re-test cold start, both directions and physical holding before adoption.

### 4. Power the USB interface only when needed

For a board revision, feed U3 from USB VBUS or a separate switched supply, with isolation for UART and reset-related signals so neither domain powers the other through I/O pins. WCH's separate-supply reference circuit discusses this issue. Moving VCC alone is incomplete. A removable programming adapter is another option because the MCU is already programmed in the Uno.

If U3 is already truly suspended, this saves relatively little; prioritize the measured larger load. If it stays active, removing its battery load could be a major improvement.

### 5. Reduce remaining sensor current after the large loads are addressed

For a revised board, consider roughly 100 kOhm external reed/tilt pull-ups with internal pull-ups disabled, plus appropriate noise filtering. At 5 V a closed 100 kOhm contact path draws about 50 microamps. Validate cable noise and reliable wake; greater resistance trades current for noise margin. Keep a wake-capable path connected for the armed lid.

A useful design target is **below 0.2 mA total measured sleeping current**, with minimal false wakes. This is a target requiring the full board and mechanics to cooperate, not a promised battery life. At 0.2 mA, ideal standby consumption is about 144 mAh per 30 days, before play, motion, alarms, self-discharge and voltage limits.

## Tilt-only wake option

The tilt switch is already enabled as a wake source. It is a passive contact, so its steady drain is primarily the D3 pull-up: approximately 0.10–0.25 mA when closed at 5 V, and only leakage when open. That is less than the current closed-lid path's combined 0.60–0.75 mA. These estimates use the board wiring and the MCU pull-up range in the Microchip datasheet linked above; they are not measurements.

Merely disabling the lid interrupt saves little if A5 and A3 still feed the closed reed. A tilt-only mode must also disable the lid's pull-up paths, then restore and settle lid sensing before using its reading on wake. The tilt pull-up must remain enabled (or be replaced with a suitable external pull-up).

The recommended compromise is conditional sleep:

| Situation | Proposed wake policy | Reason |
|---|---|---|
| Disarmed, startup complete, lid closed, claw commanded closed | Tilt-only; turn off both lid pull-up paths during sleep | Removes roughly 0.60–0.75 mA from the present closed-lid sensing load without losing an armed alarm or a pending relatch |
| Armed | Tilt plus lid, using one weaker lid pull-up | Preserves detection if someone opens the lid without moving the base enough to trip the tilt switch |
| Startup waiting for lid closure, or claw open / relatch pending | Retain lid wake | Closing the lid must still wake the MCU to finish startup or relock |

An all-states tilt-only policy could miss a gently opened lid while armed, or a lid closure while the claw is open. Tilting the base and moving the lid are not guaranteed to produce the same sensor event. The conditional policy avoids that behavior change. Mounting the tilt so its contact rests open can further reduce its static pull-up current, but reliable wake on a gentle deliberate movement must be verified in the assembled box.

This conditional policy is implemented in v8.5; v8.4 enabled both wake inputs in every sleep. It does not address any standby current from the servo or USB interface.

## Suggested next implementation

Start with total-current and servo-disconnected measurements. Then implement conditional tilt-only wake for a disarmed, closed, locked box; retain lid wake with reduced pull-up current for armed or pending-closure states. Combine this with sleep-only BOD disable and wake diagnostics/filtering. If the servo or CH340 remains a milliamp-level load, the hardware supply changes will matter much more than further MCU register tuning. The current firmware already implements deep sleep, so firmware alone cannot guarantee months of life on this wiring.

## Original rename/review verification (before v8.5)

- Renamed canonical sketch to `treasure_box/treasure_box.ino` and packaged the distribution in `output/treasure_box/`.
- Updated test/build/documentation references; kept historical release/backup paths with explanatory notes.
- All three firmware files are byte-for-byte unchanged. The renamed Uno build produces an Intel HEX identical to the archived, previously verified v8.4 release: **20,004 bytes flash, 547 bytes static SRAM**.
- All **136 host firmware scenarios**, including the 36,000-action campaign and sleep cases, passed, along with 396,021 audio plans and 21 waveform cases. These are logic/model checks, not electrical current measurements.
- No new firmware was uploaded and no power-saving behavior was changed during this recommendation-only review. Logs are in `output/build/rename-review-2026-10-16/`.
