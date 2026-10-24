# Liam's Knock-to-Unlock Treasure Box

Firmware **v8.5**. This guide supersedes the older PDF manuals.

Knock on the wood beside the little cluster of nails on the lid, right of center. The round lid button arms the alarm. The toggle immediately to the right of the keypad controls power.

## Power-up and claw controls

- **Power on with the lid closed:** leave it shut. After one full second of a closed-lid signal, the box explicitly commands the claw closed. Allow the 1.5-second closing command to finish before entering keys.
- **Power on with the lid open:** no claw command is sent. Close the lid; after it stays closed for one second, the claw is commanded closed. Reopening during the wait or initial closing movement stops the command and restarts the wait.
- Startup ignores knocks and buttons until this first closure completes. Release any held keys or lid button before normal use.
- A correct knock or the four-key bypass (factory default `ABCD`) opens the claw.
- **Tap and release `#` to close the claw**, even if the lid is open. This clears the accepted-knock/access state. The short action runs on release so it can be distinguished from a hold.
- After an open lid is closed, the claw normally closes after about two seconds.
- With the alarm off, a correct knock or ABCD gives you 10 seconds to lift the lid. With the alarm on, the PIN-entry countdown runs first; the claw-opening timer waits. Press and release the lid button to start a fresh ten-second PIN window. A correct PIN then gives you ten fresh seconds to lift the lid. A PIN timeout closes the claw immediately and leaves the alarm armed. Repeating ABCD does not extend either window.

v8.3 corrects the closing signal for this box’s Proton gripper servo. Keep fingers clear while it moves. For the first check after installation, start with the lid shut, wait for startup to finish, then use ABCD followed by a tap-and-release of # without first opening and closing the lid.

## Treasure music and the alarm

| Situation | Behavior |
|---|---|
| Alarm off + correct knock or bypass | Claw opens, green comes on, and rising chest music begins. |
| Lid opens after disarmed access | The treasure reveal fanfare plays. |
| Alarm on + correct knock or bypass | Claw opens. Red and green stay on; blue stays off. The two-hit monster heartbeat speeds up from about 60 to 190 beats per minute. It is an urgency cue, not a one-second counter. Enter the alarm PIN within ten seconds. Pressing and releasing the lid button restarts this PIN-entry window. |
| Correct PIN within that window, lid still closed | Alarm disarms with a short success chime, and accepted access is retained. A fresh ten-second opening window starts when the PIN is accepted. Anticipation follows the chime; open the lid for the fanfare. |
| Correct PIN while the lid is already open | Alarm disarms with a short success chime, then the reveal plays. |
| No PIN within ten seconds | The heartbeat stops, the claw closes, and a descending “wo wa wa wa waa” cue plays. The alarm stays armed. |
| Three wrong PINs during the accepted-access heartbeat | Access is canceled in the same way: claw closes, failure cue, still armed. |
| Lid opens while armed without accepted access | An intrusion countdown begins. Timeout or three wrong PINs starts the frantic repeating alarm melody that grows faster, higher, and louder over its first 30 seconds, then holds at its maximum intensity. |

`ABCD` is a **knock bypass, not an alarm-disarm code**. It no longer disarms the alarm or silences an active alarm. Enter the alarm PIN to disarm. Repeating `ABCD` does not extend an existing countdown.

Tap `#` to cancel accepted access and return to the locked state. A real intrusion countdown or sounding alarm still requires the PIN. Blue stays off while armed except during the explicitly requested blue/green knock-programming feedback.

## Arming and disarming

- Press and release the illuminated arm button to arm a disarmed box.
- Enter the stored PIN on the keypad to disarm. No submit key is needed; the firmware knows the saved PIN length.
- Press and release the lid button while armed to start a fresh ten-second PIN prompt. If you already opened the claw with a knock or ABCD, this restarts the PIN window and keeps the claw open. An unused prompt closes an unlocked claw and leaves the alarm armed; an idle prompt on an already locked box simply ends. Pressing the button does not postpone an intrusion countdown or silence a sounding alarm.
- A short tap on `*` clears partially entered PIN digits. After five seconds without a PIN digit, the next digit starts a fresh entry.
- Press one key at a time. Multiple simultaneous keys are ignored until all are released. Tap and release every PIN digit, especially `0`: holding `0` is the music shortcut.
- Disarming plays a short success chime and starts ten fresh seconds to lift a still-closed lid after accepted access. If you already opened and reclosed the lid, normal two-second relocking applies.
- Holding the arm button no longer programs knocks.

The factory PIN is `1234`. Existing saved PINs and knock patterns are retained by this update.

Example: knock, wait five seconds, press and release the lid button, wait seven seconds, then enter the correct PIN. The claw stays open throughout. You then have ten new seconds to lift the lid.

## Teach a new knock

1. Enter the bypass (`ABCD` unless previously customized).
2. Press and hold `#` for three seconds. Do not enter another keypad key in between.
3. Blue and green slowly pulse: the **ten-second programming window has started**.
4. Release `#`, then knock the wood beside the small nail cluster on the lid, right of center: 2–10 knocks, with each gap shorter than 1.5 seconds.
5. **Press `#` again within that same ten-second window to save.**

Five quick blue/green flashes and a confirmation tone mean the new knock was saved. No confirmation, too many knocks, an empty recording, or a confirmation after the deadline keeps the old pattern. One quick blue/green flash and a low tone mean restart the process.

The three-second initiating hold does not save a recording. You must release it and press it again. The old bypass-then-short-`#` recording shortcut is gone: a short press closes the claw.

## Change the alarm PIN

1. Hold `*` for three seconds.
2. Red and green slowly pulse: the **ten-second programming window has started**.
3. Release `*`, then enter a new PIN containing **4–8 digits**.
4. **Press `*` again within that ten-second window to save.**

Five quick red/green flashes and a tone confirm the save. A ninth digit, timeout, or confirmation with fewer than four digits cancels the edit. One quick red/green flash and a low tone mean start again; the previous PIN remains unchanged. Pressing `#` cancels this edit and closes the claw.

As requested, this reset method does not require the old PIN. Changing the PIN does not itself disarm the alarm; enter the newly saved PIN afterward if needed. During programming, the setup lights replace normal status lights. A pending alarm countdown resumes with a fresh ten seconds after setup feedback; an already sounding alarm resumes sounding.

## Choose your music

1. With no alarm countdown or sounding alarm, **hold `0` for three seconds**.
2. **Release `0`. Only the green lights slowly pulse.**
3. Within ten seconds of the pulsing starting, press one option:

| Key | Theme | Character |
|---|---|---|
| **1** | Zelda-style Dungeon | Magical chest build, treasure fanfare, ominous monster alarm |
| **2** | Pokémon-style Trainer | Bright adventure phrases, uplifting victory, urgent encounter alarm |
| **3** | Mario-style Arcade | Bouncy rhythms, playful victory, frantic arcade alarm |

The green lights flash the selected number (one, two, or three), and that theme's victory tune confirms the saved choice. The theme survives power-off and applies to startup, keypad sounds, anticipation, reveal, disarm, failure, countdown heartbeat, and the escalating alarm. These are original melodies inspired by each theme.

Tap `*` to cancel and keep the old theme. Tap `#` to cancel and close the claw. An invalid choice or waiting ten seconds leaves the old theme unchanged. Selecting music never opens the claw, disarms the alarm, or extends the opening timer. The alarm remains armed even while its red light is temporarily replaced by the green-only picker; a real lid intrusion interrupts the menu. Disarm before changing music if a countdown or alarm is active.

## Sleep and wake

After five minutes without activity, the MCU sleeps and all lights turn off, whether the alarm is armed or disarmed. **Gently tilt the box to wake it.** When disarmed with the lid shut and the claw commanded closed, tilt is the only wake source. While armed, the lid sensor also remains active: opening the lid still wakes the box and starts its intrusion countdown. Lid wake is also retained whenever closure or relocking is pending. Saved settings and the armed state are retained.

The keypad and lid button are not electrical wake sources, so tilt first if the lights are asleep. Active countdowns, music, setup, held controls, and a sounding alarm keep it awake. An open box can sleep too. At open-lid startup, the box can sleep while waiting for its first closure; closing the lid wakes it to finish startup.

## Power loss and hardware limits

The knock, PIN, bypass, armed state, and music theme are stored in EEPROM. Unconfirmed programming edits are only in memory and are discarded on timeout or power loss. Confirmed knock and PIN saves use two storage slots so an interrupted save can fall back to the previous valid setting. Valid old four-digit PINs automatically migrate to the new PIN storage.

The installed three-wire goBILDA actuator cannot report physical position. Claw prying while the lid stays closed cannot be detected, and this build has no spoken intrusion warning. The lid alarm remains available. The alarm now also ramps speaker drive from a quieter duty cycle to the previous full-level duty cycle. The volume change needs a listening check on the assembled box.

Sleep lowers MCU and LED consumption; it does not disconnect the servo supply or USB interface chip. Actual battery life and reliable wake-up must be checked on the assembled PCB.
