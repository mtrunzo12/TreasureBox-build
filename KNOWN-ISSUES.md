# Read before ordering

- **Historical hardware:** these are the existing A0 source and fabrication files. They were copied intact and have not undergone a new fabrication audit for this kit. Files do not prove a board is order-ready.
- **USB-C:** the owner reports the connector was designed backwards. This build's documented workaround is to program the removable ATmega328P in a compatible DIP Uno. Do not rely on the board's USB-C for programming without resolving the hardware issue.
- **Parts:** exported BOMs list values, quantities and footprints, not vetted manufacturer/supplier order numbers. Check footprint pitch, voltage/power ratings, capacitor polarity, transistor pinout and cable pin numbering.
- **Power:** battery and servo behavior still need physical testing. MCU sleep does not disconnect the servo or USB interface. Review firmware/POWER_REVIEW.md; battery-life numbers are not guaranteed.
- **Claw:** the installed servo's published pulse range is 600–2400 µs. Source endpoints are for the original linkage. Test with the mechanism clear and tune before attaching it to a lid; a three-wire servo does not confirm its actual position.
- **Knocks:** the piezo location and chest material change readings. Mark's build needed a marked knock area and further mechanical tuning. Read measurements rather than copying a threshold blindly.
- **Programming:** keep the primary .ino and its folder named treasure_box, alongside both .h files. Confirm chip bootloader/fuses and the custom PCB pin map before uploading.
