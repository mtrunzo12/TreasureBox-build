# Treasure Box firmware

Current verified release: **v8.5**. The active Arduino sketch uses a stable name so its folder does not become misleading as releases advance.

| Location | Purpose |
|---|---|
| `treasure_box/treasure_box.ino` | Canonical sketch; open this in Arduino IDE |
| `treasure_box/audio_driver.h` | Speaker driver required by the sketch |
| `treasure_box/music_themes.h` | Music scores required by the sketch |
| `output/treasure_box/` | Matching, self-contained distribution sketch |
| `output/build/v8.5/` | Current release image and verification logs |
| `output/backups/` | Local historical source/device backups; private and ignored by Git |

The sketch folder and its primary `.ino` must have the same name. On 2026-10-16, `treasure_box_v7` was renamed to `treasure_box`, and the loose distribution files were placed in `output/treasure_box/`. That rename alone preserved the v8.4 image byte-for-byte. The subsequent v8.5 release implements conditional tilt-only sleep and reduces reed pull-up current. Historical build and backup names remain as originally recorded.

Build and test from this directory:

```sh
arduino-cli compile --fqbn arduino:avr:uno --warnings all treasure_box
python3 tests/run_tests.py
```

- [User manual](USER_MANUAL.md)
- [Developer guide and upload instructions](DEVELOPER.md)
- [v8.5 quality report](QUALITY_REPORT_v8.5.md)
- [Battery and sleep-power review](POWER_REVIEW.md)
