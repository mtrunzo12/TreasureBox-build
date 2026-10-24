# Knock-to-Unlock Treasure Chest

Build a toy chest with a saved knock rhythm, keypad PIN, servo claw, lights and music. This folder is a curated snapshot of Mark and Liam's build. Start small: get the inputs working on a breadboard before ordering boards.

## Start here

1. Install [Claude Code](https://code.claude.com/docs/en/setup) or [Codex](https://developers.openai.com/codex/quickstart/), plus [Arduino IDE](https://www.arduino.cc/en/software/) and [KiCad](https://www.kicad.org/download/).
2. Download and unzip this build kit. Open the whole `treasure-chest` folder in your AI coding app. From [GitHub](https://github.com/mtrunzo12/TreasureBox-build), use Code → Download ZIP; no terminal or Git knowledge is required. Forking is optional and gives you your own place to save changes.
3. Paste the prompt from `START-HERE.md`. Ask the assistant to explain the next small step, then stop for your measurements or a photo. Do not ask it to order parts or fabricate a board before the checks below.

## What is included

- `firmware/treasure_box/`: current v8.5 sketch and its two required headers. Keep all three files in this folder.
- `firmware/USER_MANUAL.md`: using the toy and recording knock/PIN/music settings.
- `firmware/DEVELOPER.md`: exact pin map, dependencies, programming workflow and mechanical limits.
- `treasure_box_main/`: main-board schematic, layout, KiCad project and original exported BOM.
- `treasure_box_frontpanel/`: panel-board schematic, layout, KiCad project and original exported BOM.
- Each board folder includes the historical A0 Gerber/drill package and individual fabrication files.
- `PARTS.csv`: combined exported PCB component lists (44 placements across both boards). These are design values/footprints, not a verified supplier cart. The off-board parts are listed below.
- `KNOWN-ISSUES.md`: read this before spending money.
- `CHECKLIST.md`: a build sequence for a parent and child.
- `SOURCE-MANIFEST.json`: snapshot and source hashes. No family photographs, device backups, EEPROM dumps, credentials, local history or old build logs are included.

## Ordering the boards

This is the as-built A0 design, not a newly reviewed production release. The owner reports a backwards USB-C design; the build uses a removable ATmega328P programmed in a compatible DIP Uno. Resolve and review that issue before treating this as a plug-in USB design.

1. Open the `.kicad_pro` in each board folder. Check component orientation, connector pin numbering, footprint fit, board outline and hole locations against your parts and chest.
2. Run KiCad's electrical and PCB design-rule checks. Review their findings; a clean result alone does not prove the connector or physical wiring is correct.
3. Decide whether you are intentionally reproducing A0 with the Uno programming workaround or correcting the hardware first. If changed, regenerate Gerber and drill files from that exact saved revision. Do not order the historical ZIP for a revised board.
4. Upload **one ZIP per board** to JLCPCB. Confirm both copper layers, masks, silkscreen, board outline, drill holes and displayed dimensions in the preview. Use the [JLCPCB KiCad export guide](https://jlcpcb.com/help/article/how-to-generate-gerber-and-drill-files-in-kicad-9).
5. Choose bare PCB fabrication if you want to solder it yourselves. This package is not a turnkey assembly order; it does not include verified supplier part numbers or placement data. Check quantity, material, thickness and finish against the actual design before paying.
6. Check the delivered bare boards for obvious defects and shorts before populating them.

## Off-board parts to plan for

A wooden chest; piezo knock disc; reed switch and magnet; tilt switch; 4×4 keypad; passive speaker; illuminated arm button; red, green and blue lights with the appropriate wiring/resistors; four-AA holder and cells; main power switch; matching 16-way ribbon connectors/cable; keypad harness; wire, mounting hardware and strain relief. The installed claw is ServoCity 3219-0002-0002 with a goBILDA Proton Torque 2002-0180-0002 servo. Verify all electrical ratings and connector pitches before buying.

Tools: soldering iron, solder, ventilation, wire cutters/strippers, multimeter, breadboard/jumpers, and a compatible socketed ATmega328P Uno for the documented programming workaround. An adult handles soldering and initial powered claw tests. Keep fingers clear of the mechanism.

## Firmware

Open `firmware/treasure_box/treasure_box.ino` in Arduino IDE. The documented target is Arduino Uno / ATmega328P, Arduino AVR Boards 1.8.8 and Servo 1.3.0. Use the developer guide for the correct `BENCH_WIRING` choice; the custom PCB uses `false`, even when its removable chip is programmed in an Uno. Review the actual pin map before loading it onto a different breadboard arrangement.

An unprogrammed ATmega328P may need its bootloader and fuse configuration prepared first. Do not assume every purchased chip arrives Uno-ready. Power everything off before transferring a socketed chip, and check its notch orientation.

The code was built and tested on the original setup. Knock sensitivity, mechanical endpoints, wake behavior and battery draw still need testing on your own assembled chest. This is a toy latch, not a security lock.
