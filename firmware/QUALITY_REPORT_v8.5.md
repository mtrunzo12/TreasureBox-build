# Treasure Box v8.5 — conditional sleep power reduction

Release date: 2026-10-16. Target: ATmega328P on the custom PCB, programmed in the user-confirmed USB Uno at `/dev/cu.usbmodem11301`. The stable Arduino sketch path is `treasure_box/treasure_box.ino`.

## Changes

- After five idle minutes, a disarmed, initialized box with a stable closed lid and a completed closed-claw command uses **tilt-only wake**. A3's internal pull-up is disabled; A5 is driven LOW so R6 holds the otherwise unused lid input at a defined LOW without pull-up current. The lid's interrupt mask/group is disabled in this mode.
- Armed operation, an open lid, startup waiting for closure, and an open claw retain **tilt plus lid wake**. A3 retains its internal pull-up. A5 becomes an input without a pull-up, removing the external 10 kOhm pull-up supply without driving the reed input LOW.
- Tilt D3 retains its pull-up and either-edge wake in both modes. No periodic wake timer is added.
- Input levels are checked before and after sensor reconfiguration with interrupts disabled. An edge can cancel sleep; a pending enabled pin-change interrupt wakes the adjacent SEI/SLEEP sequence.
- On wake or canceled entry, normal sensor power and A3's pull-up are restored before ADC/lid processing. The existing logical lid baseline is retained, so genuine opening/closing events are processed after settling rather than erased.
- The sleep log states which wake policy is selected. Startup reports v8.5. Ordinary timing, music, PIN/knock storage, servo endpoints and motion behavior are unchanged.

Calculated saving in the closed-lid sensing circuit at a 5 V logic rail: approximately **0.60–0.75 mA in tilt-only sleep**, or **0.50 mA when retaining lid wake**. These are calculations from the schematic and pull-up specifications, not measurements of total device current. Servo and CH340C supplies remain powered. BOD changes and nuisance-wake filtering remain future recommendations.

## Tests and build

- **144 actual-sketch host scenarios passed** with undefined-behavior and bounds instrumentation, including the existing 36,000-action child-interaction campaign.
- New regressions cover actual lid restoration after tilt-only sleep, a locked but open lid retaining wake, reed-close relatching, rearming after tilt-only sleep, lid/tilt races before and after pull-up switching, and armed open-lid startup without a false closed transition.
- Existing tests verify both resting tilt levels, armed intrusion after lid wake, startup closure, five-minute boundaries, clock wrap and interrupt-mask restoration. Updated assertions verify the correct pull-up and interrupt configurations inside sleep.
- The host lid model now represents R6 grounding A3 when both pull-ups are off, so tests can detect accidental interpretation of the sleeping circuit as a real closed lid.
- **396,021 audio frequency/duty plans and 21 waveform/stop cases passed.** Servo stop-phase coverage remains in the full suite.
- Uno build: **20,266 bytes flash; 547 bytes static SRAM**, leaving 1,501 bytes for locals/stack. Four existing Arduino-core unused-parameter warnings; no sketch warnings.

## Device and release records

Before updating, the full 32,768-byte MCU flash was saved. All 20,004 v8.4 application bytes matched the archived verified release. Source, tests and manuals were also backed up before editing.

**Uploaded successfully: all 20,266 application bytes were read-back verified by avrdude.** The serial startup banner reports v8.5 and read-only status responds. The upload requested flash only, with no EEPROM write or chip erase.

The programming Uno reports an open lid and waits for the first closure, as expected with its sensor inputs unconnected. That is not a physical PCB sensor test.

- `output/backups/before-v8.5/`: source/tests/manuals and pre-update MCU flash.
- `output/build/v8.5/`: HEX/ELF, source SHA-256, targeted/full tests, compile, upload and serial logs.
- `output/treasure_box/`: matching distribution sketch including both required headers.

## Physical checks after installation

1. With the alarm off and the lid shut/locked, wait five quiet minutes. Lights go off. Tilt gently and confirm normal operation returns.
2. Repeat with the alarm armed. Opening the lid must still wake the box and start the intrusion countdown; test gentle lid movement without deliberately tilting the base.
3. Let an open, disarmed box sleep. Closing the lid must wake it and relock after the normal delay. An open-lid power-on must also retain closure wake.
4. Check reliable lid wake on the actual wires. The weaker internal pull-up has less noise margin than the former 10 kOhm external pull-up.
5. Measure battery-only current after sleep. Compare armed/disarmed states and, with power disconnected before rewiring, the servo-disconnected case. This verifies the savings and identifies any larger external load.

Host tests and flash verification do not measure real current, noise, mechanical clearance or battery life. Disconnect USB and box power before transferring the MCU.
