# Parent build checklist

## Plan
- [ ] Read README.md and KNOWN-ISSUES.md together.
- [ ] Choose a chest and sketch the claw, keypad and electronics positions.
- [ ] Install your AI helper, Arduino IDE and KiCad; open this whole folder.
- [ ] Ask the helper to explain the exact parts and pin map. Confirm purchases against the BOM and connector footprints.

## Prototype
- [ ] Read the knock piezo on a breadboard; record quiet readings and deliberate taps.
- [ ] Check the reed switch in both lid positions and test the tilt switch.
- [ ] Check every keypad key.
- [ ] Test the speaker and each light.
- [ ] Test the servo with the claw clear, using the right supply and a shared ground. Confirm its specified travel limits.
- [ ] Combine the inputs only after each works alone. Save the working pin map and code.

## Make the boards
- [ ] Open both KiCad projects and compare the circuit to the prototype.
- [ ] Resolve the USB-C design question and choose the programming method.
- [ ] Verify footprints, component orientation and ribbon/keypad pin order with the physical parts.
- [ ] Run checks; export and review Gerber and drill previews from the exact revision being ordered.
- [ ] Order one main-board design and one panel-board design as bare PCBs.
- [ ] Assemble with adult help. Inspect joints and check for power-to-ground shorts before applying power.
- [ ] Power and test the boards on the bench before mounting them.

## Install and finish
- [ ] Program the correct chip with the three-file v8.5 sketch; power off before transferring a socketed chip.
- [ ] Mount the boards and battery with clearance and strain relief. Insulate exposed connections.
- [ ] Attach and align the claw without forcing the servo against a stop.
- [ ] Fit the piezo, lid magnet/reed, keypad, button, speaker and lights.
- [ ] Record a reliable knock rhythm and PIN using the user manual.
- [ ] Test correct/wrong knocks and PINs, lid-open alarm, closing/relocking and all music choices.
- [ ] Wait five idle minutes and test wake behavior, both armed and disarmed.
- [ ] Repeat with battery power alone. Check current, pack voltage under movement and any unexpected heating.
- [ ] Let your child try it while you watch; adjust the knock area and claw clearance until use is repeatable.
