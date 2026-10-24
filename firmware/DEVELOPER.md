# Treasure Box Firmware — Developer Guide

## Current release

**v8.5**, uploaded and read-back verified (20,266 bytes flash; 547 bytes static SRAM). Canonical source is the three-file sketch in `treasure_box/`: `treasure_box.ino`, `audio_driver.h`, and `music_themes.h`. The sketch folder and primary `.ino` now share the version-neutral name `treasure_box`. All three have synchronized distribution copies in `output/treasure_box/`.

- `BENCH_WIRING=false`: custom PCB pin map, including when the removable MCU is programmed in an Uno.
- `DEBUG=false`: production configuration; read-only serial `s` status remains available.
- Arduino AVR Boards 1.8.8; Servo 1.3.0; target `arduino:avr:uno`.
- Final resource usage, test counts, upload verification, and child-interaction audit are recorded in `QUALITY_REPORT_v8.5.md`.
- Markdown manuals reflect v8.5. The v8.4 illustrated trifold remains applicable: tilt gently to wake. Older User Manual and Developer Guide PDFs contain obsolete instructions.
- Mechanical clearance, physical lid/tilt sensing, speaker sound, and sleep current require assembled-box checks.

## Physical direction correction

The exact gripper is ServoCity **3219-0002-0002**, supplied with a goBILDA **Proton Torque 2002-0180-0002**. Its published pulse range is **600–2400 microseconds**, not the 500–2500 range previously assumed. v8.2's 2444-microsecond close pulse exceeded that specification. The gripper assembly procedure sets the servo clockwise, then backs off about two degrees before installing the horn with the jaws closed. At 0.098 degrees/microsecond, that corresponds to about 2380 microseconds.

| Command | Sketch constant | Pulse |
|---|---|---:|
| Closed | `SERVO_CLOSED_US` | 2380 microseconds |
| Open | `SERVO_OPEN_US` | 1056 microseconds |

The opening direction and inward adjustment remain unchanged. Rotating the entire assembly upside down does not reverse the electrical endpoint mapping. Horn/linkage alignment still determines the physical jaw position; neither source tests nor flash verification measure that position.

Sources: [kit](https://www.servocity.com/servo-driven-gripper-kit-servo-included/), [Proton specifications](https://www.gobilda.com/proton-servo-steel-gears-180-rotation-torque-ratio/), [assembly step 5](https://www.servocity.com/content/user_manuals/3219-0002-0002_assembly_instructions.min.pdf).

Startup powers the sensor, enables the A3 pull-up, waits 150 ms, discards ADC channel-settling samples, and debounces the lid. No claw pulse is sent with the lid open. A full second of continuously closed lid readings starts an unconditional 2380-microsecond close command. Valid pulses continue for 1500 ms, with lid polling, serial status and watchdog servicing still running. Any open raw/debounced lid reading stops the drive and restarts the waiting process. Only after the whole drive window finishes do normal inputs become available; held controls must first be released. Startup does not infer a measured claw position.

Normal `moveLatch()` commands retain their 600-ms drive window. `startLatchDrive()` atomically attaches Servo and sets the target, avoiding a default center pulse. `stopLatchDrive()` waits for the current HIGH pulse to finish, then atomically detaches while LOW so the last closing pulse cannot be shortened into an opening command. It polls at most 350 times with 10-microsecond delays (plus execution overhead) before a defensive forced stop. Tests cover every one-microsecond stopping phase of both endpoint pulses and a stuck-HIGH fallback. The final pulse may add up to about 2.4 ms to normal drive duration. `latched` records a completed command, not feedback.

Short `#` no longer depends on the lid sensor: it explicitly closes the claw on key release. This also avoids the apparent no-op when an open lid was reported.

## Hardware and pins

ATmega328P-PU, external 16 MHz crystal, 5 V logic. Passive speaker/transistor stage on D11; servo signal D10, separately routed `SERVO_V` supply.

| Pin | Function | Behavior |
|---|---|---|
| D2, D4, D5, D6 | Keypad rows | One row driven low at a time; inactive rows are inputs |
| D7, D8, D9, A4 | Keypad columns | Input pull-up |
| D3 / PCINT19 | Tilt/wake switch | Input pull-up; either level change wakes |
| D10 | Servo | 600–2400 µs configured range; pulses only during a move |
| D11 / PB3 | Speaker | Custom Timer2 duty-controlled output |
| D12 | Red LED + arm-button ring | Active high |
| D13 | Blue LED | Active high |
| A0 | Green LED | Active high |
| A1 | Knock piezo | Analog input; internal 1.1 V reference |
| A2 | Arm button | Input pull-up, active low |
| A3 / PCINT11 | Lid reed | Closed low, threshold 512; internal pull-up and wake retained only when needed during sleep |
| A5 | Reed pull-up supply | HIGH awake; INPUT without pull-up for lid-wake sleep; OUTPUT LOW for tilt-only sleep |

The blue/green and red/green breathing effects use software PWM in `ledTick()` because these LED pins do not provide available hardware PWM. Alarm and ordinary music remain monophonic.

## State and timing

All ordinary input scanning, hold gestures, alarm deadlines, knock listening/recording, programming feedback, and music advance from the main loop. Normal servo movement blocks for 600 ms plus the remainder of its final pulse; the 1500-ms startup drive remains cooperative. The watchdog is reset each loop; no key hold or continuous piezo noise can create an unbounded waiting loop.

Access and arming are independent. `accessGranted` records a valid knock/bypass; `armed` remains unchanged. The stored bypass is a knock substitute, not a privileged disarm command.

| Alarm state | Purpose | Expiration |
|---|---|---|
| Idle | Armed or disarmed normal operation | None |
| Calm | Arm-button PIN prompt | 10 s: still armed, stop music |
| Access | Valid knock/bypass while armed | 10 s: close claw, clear access, retain armed, descending failure cue |
| Challenge | Unauthorized lid-open edge | 10 s or three wrong PINs: sounding alarm |
| Sounding | Escalating ominous motif | Stored PIN disarms |

Three wrong PINs in Access mode also close the claw with the failure cue. Repeated bypasses do not restart a deadline or downgrade a sounding intrusion alarm. `#` cancels an Access prompt and closes the claw; it does not dismiss an intrusion Challenge/Sounding state.

Correct PIN entry preserves pending accepted access and starts a fresh opening window. A short success chime plays first. If the lid is shut, anticipation follows; if already open, the reveal follows. Blue is off during armed operation, with the explicit exception of knock programming and its requested feedback. While armed and unlocked, green remains on.

With the alarm off, unused access times out ten seconds after completion of the opening command. While armed in Access, Calm, or Challenge, automatic relatching waits for the PIN window to finish. An explicit lid-button press/release starts a fresh ten-second PIN window in Idle, Calm, or Access; it retains Access so timeout closes the claw, and it does not reset wrong-PIN attempts. A correct PIN starts ten fresh seconds for unused access. Repeated ABCD does not restart either timer. Intrusion Challenge/Sounding cannot be postponed with the button. Once a disarmed lid is actually open, the unused-access timer no longer applies. A used box relocks two seconds after the lid closes. Relocking clears access and recording eligibility. In-progress programming and its brief feedback postpone automatic relocking until they finish.

## Countdown, success chime, and alarm audio

The two-hit heartbeat starts at 60 BPM and accelerates toward 190 BPM by the ninth second. Each new beat interval is derived from the original alarm clock; it never moves the ten-second deadline. Pulse spacing scales with the beat interval. The sound is now an urgency cue rather than a seconds counter.

A correct disarm PIN plays a four-note original success chime (about 395 ms in theme 1). The alarm turns off immediately. Pending anticipation or reveal follows the chime, including when the lid opens during it. Manual `#`, rearming, and the access deadline remain effective during the chime. Repeated ABCD no longer interrupts an in-progress reveal.

Each theme supplies a different 24-note urgent alarm motif. Over 30 seconds its note durations shorten from 100% to 50%, spacing falls from 20 to 5 ms, pitch accents rise, and requested speaker duty rises from 12% to 50%. The ramp then holds; repeated bypasses do not restart it.

`audio_driver.h` owns Timer2 instead of Arduino `tone()`. Compare A defines each waveform period and compare B ends its high pulse on D11/PB3. The prescaler is chosen for a complete period of at most 256 ticks. Duty is capped at 50%, the old square-wave duty. The interrupt counts complete periods and stops with the transistor drive low. Reconfiguration is atomic and clears stale compare flags. Timer0/millis and Timer1/Servo remain separate. Do not call Arduino `tone()` or `noTone()` in this sketch: they would claim the same timer vector. This implementation targets ATmega328P at 16 MHz.

The timer design follows Microchip's [CTC mode description](https://onlinedocs.microchip.com/oxy/GUID-74F8229E-4C43-4FA0-BE7D-1AA303C6F8A4-en-US-6/GUID-1EB44C0C-12C2-45B2-8B32-CEE5963C9F73.html) and [output compare unit description](https://onlinedocs.microchip.com/oxy/GUID-74F8229E-4C43-4FA0-BE7D-1AA303C6F8A4-en-US-6/GUID-EA3A318D-5E79-4663-9D04-78E84CC7C89A.html). The local schematic connects PB3 through R2 (1 kOhm) to S8050 Q1, with R3/R4 (22 Ohm each, parallel) in the speaker path. Duty control reduces average drive at the start; it cannot establish actual sound-pressure levels without measuring the assembled speaker.

## Music themes

`music_themes.h` stores all three original scores in PROGMEM: Zelda-style Dungeon, Pokémon-style Trainer, and Mario-style Arcade. Each has different startup notes, keypad/UI notes, anticipation pitches and rhythms, reveal, disarm, failure, heartbeat timbre/pulse spacing, and alarm motif. Theme 1 retains the v8.3 score. All share the requested 60-to-190 BPM heartbeat ramp and 30-second alarm intensity ramp.

Hold `0` three seconds outside programming and active alarm states, release, then press `1`, `2`, or `3` within ten seconds. Green alone pulses; choosing saves EEPROM byte 120, flashes green one/two/three times, and plays that theme's reveal as `MUSIC_THEME`. The preview never grants access or disarms. `*` cancels; `#` cancels and closes, including during confirmation flashes. Invalid keys do not extend the deadline. Timeout is processed before keys. Real intrusion overrides the menu/preview; an existing relatch deadline is not extended.

A normal `0` digit is processed on release, so holding the final zero of a PIN cannot accidentally disarm before opening the menu. Other digits retain press handling. PIN-programming mode still accepts zero normally. Menu completion/cancellation requires held controls to be released before normal actions resume.

## Sleep implementation

The [battery review](POWER_REVIEW.md) explains the measured-versus-estimated limits and remaining board loads. v8.5 implements conditional wake selection and reduced reed pull-up current; the other recommendations remain future work.

`serviceTilt()` refreshes inactivity on either level change. After 300,000 idle milliseconds, `maybeSleep()` allows power-down in armed or disarmed Idle, including an open lid or startup waiting for closure. Held controls, startup closing, active knock capture, menus, programming/feedback, music, and non-idle alarm states prevent sleep.

D3 uses PCINT19 (group 2, mask bit 3), and retains its internal pull-up and wake on either edge in every sleep mode. Lid policy is selected before power-down:

| Condition | A3 reed configuration | A5 / R6 configuration | Wake inputs |
|---|---|---|---|
| Disarmed, startup complete, commanded locked, stable closed lid and matching raw digital reading | INPUT, no pull-up | OUTPUT LOW; R6 holds A3 low without pull-up current | Tilt only |
| Armed, open lid, startup pending, or not commanded locked | INPUT_PULLUP | INPUT, no pull-up; external 10 kOhm supply removed | Tilt + lid |

The tilt-only path disables the A3 pin-change mask and group. It never treats the deliberately grounded A3 level as a real lid reading. The two-input path retains A3/PCINT11, with the internal pull-up providing the open level. A5 is never driven LOW while lid wake is enabled.

Sleep entry rechecks the still-powered lid and tilt atomically, reconfigures the sensing circuit and wake masks, clears transition flags, and checks the enabled wake inputs again immediately before adjacent SEI/SLEEP. Both wake and canceled-entry paths restore A5 HIGH/OUTPUT and A3 INPUT_PULLUP before normal sensing, preserve the old logical lid baseline, and allow 150 ms settling. Thus a real opening/closure is handled by the normal debouncer rather than suppressed by reinitialization.

The watchdog/ADC are disabled during power-down; LEDs/audio are stopped. Interrupt masks/control, ADC and watchdog are restored on wake. The five-minute awake period remains unchanged, as do BOD handling and the servo/CH340 power supplies. No periodic wake timer was added.

Estimated closed-lid sensing savings at 5 V are 0.60–0.75 mA in tilt-only mode and 0.50 mA when retaining lid wake. These are resistor calculations, not measurements of total board current. Actual wake reliability, noise margin of the weaker pull-up and battery current require physical testing. See the [ATmega328P datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf) for pin-change wake and pull-up specifications. Keypad/button pins are not wake sources.

## Input robustness

- Startup-held keys/buttons must be released before normal interactions start.
- Multiple-key chords are discarded until full release; they cannot become a false `#` release, a partial bypass, or a recorded keypad vibration.
- A PIN pause of five seconds makes the next digit start a fresh entry; alarm deadlines are unchanged.
- Arm-button presses begun during programming or feedback are discarded.
- Runtime timestamps and the test clock use explicit 32-bit values, including wraparound tests.

## Programming flows

### Knock

`ABCD` (or the stored bypass), then hold `#` for three seconds. Eligibility lasts until another ordinary keypad key, locking, or another access method clears it; the initiating `#` hold captures eligibility when pressed. This hold does not first close the claw.

Blue/green pulse for a fixed ten-second window, beginning when the hold is recognized. The initiating key must be released. Record 2–10 knocks and press `#` to confirm before expiration. The pattern is staged separately from `secretGaps`; no EEPROM write occurs without explicit, valid confirmation. After 1.5 seconds of silence the sequence is complete; extra knocks after completion invalidate it rather than silently accepting a prefix. Keypad activity is sampled before the piezo so pressing the confirmation key is not appended as a knock.

Valid confirmation: five 80 ms on / 80 ms off blue/green flashes and a tone. Timeout, invalid/empty pattern, or overflow: one flash and a low tone, old pattern preserved.

### PIN

Hold `*` for three seconds from normal operation (including while armed), release, type 4–8 digits, and press `*` again within the fixed ten-second window. A ninth digit, insufficient length at confirmation, or timeout aborts without saving. `#` aborts and closes the claw.

Red/green pulse during entry. Success gives five flashes; failure gives one flash and a low tone. As specifically requested, no old-PIN step is required. Saving a new PIN leaves the armed state unchanged. The alarm PIN is subsequently recognized automatically when its stored number of digits has been entered; short `*` clears partial disarm entry.

Programming temporarily owns the audio/LEDs. A pending countdown is restarted for ten seconds after setup feedback; a previously sounding alarm resumes sounding. Runtime deadlines use subtraction to tolerate `millis()` wrap. The expiration check precedes confirmation processing, so a late confirmation cannot save.

## EEPROM and migration

Legacy data stays in place:

| Address | Data |
|---|---|
| 0 | Legacy layout magic `0x47` |
| 1 | Number of stored knock gaps |
| 2–19 | Nine possible signed 16-bit gaps |
| 20–23 | Legacy four-digit PIN, retained for migration/fallback |
| 24–27 | Four-key bypass |
| 28 | Armed flag |
| 32–44 | Extended PIN slot 0 |
| 48–60 | Extended PIN slot 1 |
| 64–86 | Knock slot 0 |
| 96–118 | Knock slot 1 |
| 120 | Theme: 1–3; any other value defaults to theme 1 |

Each extended slot contains a commit byte (`0xA8`), generation, length, eight digit bytes, and a 16-bit checksum. The newest valid slot wins. A confirmed PIN update writes the inactive slot and writes the commit byte last, preserving the previous valid slot if interrupted. If neither slot is valid, a valid legacy PIN migrates automatically. Valid legacy knock, bypass, and armed state are retained.

Knock candidates and PIN candidates are held in RAM until confirmation. v8.2 migrates a valid legacy knock into dual slots without changing its rhythm. Each knock slot contains commit, generation, gap count, nine 16-bit gaps, and a checksum. Saves invalidate only the destination and write the commit byte last, keeping the other valid slot available. Fault-injection tests cut every byte-write point in both save routines and require either the old setting or the fully committed new setting on reload. Legacy bytes remain intact for migration/fallback. Invalid legacy fields fall back to defaults, and an invalid nonzero armed byte is treated as armed. No credentials are printed on serial.

## Build, upload, and diagnostics

```sh
arduino-cli compile --fqbn arduino:avr:uno --warnings all treasure_box
arduino-cli upload --fqbn arduino:avr:uno --port /dev/cu.usbmodem11301 --verify treasure_box
python3 tests/run_tests.py
```

The CLI used here is bundled with `/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli`. A clean build of Arduino AVR Boards 1.8.8 has four pre-existing unused-parameter warnings in core `new.cpp`; the sketch has no warnings.

Serial at 9600 baud prints a startup banner and status. Send `s` for raw/debounced lid state, startup phase (waiting for closure, waiting one second, or actively driving closed), commanded claw position/pulse, armed state, selected theme, and tilt level. This read-only command works with `DEBUG=false`. `DEBUG=true` also enables `t` (test tone) and `c` (reveal, only while disarmed). Older debug command lists are obsolete.

The v8.2 programming Uno reported `Lid raw=1023 interpreted=OPEN startup=WAIT_FOR_CLOSED_LID claw=NOT_COMMANDED armed=YES`. This is the bare programming board's pull-up state; the MCU correctly sends no startup claw command there. It is not evidence of the actual PCB reed position. On the PCB, check `s` with the lid closed and open; closed should read near zero, open near full scale.

Host tests compile the actual sketch with simulated Arduino I/O. They cover the reported behavior, control gestures, timeouts/cancel/confirm, EEPROM migration and interrupted PIN writes, LED feedback, alarm distinctions, key/piezo noise, sleep policy, and timer wrap. They do not validate electrical waveforms, speaker acoustics, physical linkage orientation, or AVR interrupt timing. Runtime times and the host clock now use uint32_t to model AVR wraparound. ASan on this host previously hung in its runtime initialization; the runnable suite uses undefined-behavior/bounds instrumentation instead.

## Backups and release artifacts

- `output/backups/before-v8.2/`: prior source/tests/manuals and full MCU flash before this upload.
- `QUALITY_REPORT_v8.2.md`: change checklist, test results, limits, and physical acceptance checks.
- `tests/audio_driver_test.cpp`: 396,021 frequency/duty plans and 21 modeled interrupt waveform/stop cases.
- Behavior suite includes 36,000 mixed interactions across 18 prepared starting situations. The expanded campaign has a 60-second host wall-time limit; individual scenarios retain ten seconds.

- `output/backups/before-v8.1/`: v8.0 source/tests and full MCU flash read before the v8.1 update. The upload used the bootloader without an EEPROM-write operation.

- `output/backups/before-v8/`: original v7.1 source, original MCU flash, and genuine 1024-byte EEPROM captured immediately before v8. Credentials are private and the directory is ignored by git.
- This bootloader returns flash bytes for an EEPROM-read request. EEPROM was therefore read using the temporary read-only diagnostic, with no EEPROM write performed by that diagnostic.
- `output/build/v8.2/`: previous uploaded release and logs.
- `output/build/v8.3/`: previous release artifacts.
- `output/build/v8.4/`: previous verified release.
- `output/build/v8.5/`: current HEX/ELF, source hashes, test/build/upload/status logs.
- `output/backups/before-v8.5/`: v8.4 source/tests/manuals and verified full pre-update MCU flash.
- `QUALITY_REPORT_v8.5.md`: conditional sleep implementation and verification.
- `output/backups/before-v8.4/`: source/tests/manuals, trifold, and pre-update MCU flash.
- `QUALITY_REPORT_v8.4.md`: prior release results and child-interaction audit.
- `output/backups/before-v8.3/`: v8.2 source, tests, manuals and full pre-update MCU flash backup.
- `output/treasure_box/`: synchronized three-file Arduino sketch.

## Enclosure checks still needed

1. With lid shut, apply power and verify closing starts after a one-second pause and allow the 1.5-second drive to finish. With lid open, verify no startup movement; close it and verify closure after one closed-lid second.
2. Without first cycling the lid, enter ABCD, then tap and release #. Confirm 1056 microseconds opens far enough and 2380 microseconds closes the installed linkage. Repeat several cold starts; firmware tests alone do not confirm this.
3. Test disarmed knock/bypass, `#`, and lid-close relocking.
4. Test armed access: blue stays off, valid PIN starts anticipation, timeout closes with failure music while staying armed.
5. Test the actual reed-triggered intrusion countdown/alarm separately.
6. Exercise both programming flows, including cancellation, timeout, eight-digit PIN, and restoration after power cycling.
7. Verify real speaker/servo sounds do not self-trigger knocks; tune sensitivity after final mounting if needed.
8. Check five-minute sleep while armed and disarmed, tilt wake from both resting orientations, lid wake while armed, and actual battery use.
9. Check the three-second zero hold, green-only picker, all three victory previews, cancellation, and persistence across power cycling.
10. Reproduce the exact timing sequence: valid knock, wait five seconds, press/release lid button, wait seven seconds, enter PIN. Claw stays open, then grants ten fresh seconds to lift the lid.

Normal 600 ms servo movement remains blocking (plus the last pulse); tests do not certify finger clearance or response during that travel. Three-wire goBILDA position feedback is unavailable; no claw-pry detector or spoken intrusion alert is claimed. Those require a wired claw sensor/feedback path before meaningful integration.
