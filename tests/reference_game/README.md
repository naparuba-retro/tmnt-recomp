# Reference game

A complete game played with the European ROM in invincible mode (title screen to end screen, about 127 500 frames).

- `tmnt_input.txt`: pad by ranges "first last mask", frames counted from the launch.
- `tmnt_pokes.txt`: "frame address value": the RAM writes of the invincible mode.
- `tmnt_rewind.txt`: the rewinds (F9) of the game: "frame snapshot-number".
- `expected_hashes.json`: fingerprints (SHA-256 per block of 2000 lines) of the trace the port wrote during that game: screen loads, palettes, sound requests
  and APU writes (about 4 300 and 505 000 lines, no `TODO` line). Only hashes are kept, not the trace, because the APU writes are the whole soundtrack.

`bash tools/ref_run_check.sh` replays these inputs in the port alone and compares the new trace with the fingerprints (`tools/reference_hashes.py`). It
needs the European ROM as `rom/tmnt_eu.nes`. It is sensitive: removing a single key press of 8 frames around frame 60 061 makes it fail, and the report
says in which block of lines the traces start to differ. To record a new reference, replay a game, check it by other means, then run
`python tools/reference_hashes.py make build/refrun/trace.txt > tests/reference_game/expected_hashes.json`.
