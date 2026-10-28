# How exact it is

Checked by `dsp4_check` against the original chip (running in the ares emulator, its program
taken at run time from an installation outside this repository; docs/clean-room.md). Three
recordings of Top Gear 3000 with the original:

| Recording | Reads | The same here |
|---|---|---|
| 600 frames of the first race, standing | 384,185 | 384,185 (all) |
| 3,000 frames from power-on, through the menus into a race | 1,328,380 | 1,322,868 (99.6%) |
| 6,300 frames of a race, driving and steering | 3,686,679 | 3,542,671 (96.1%) |

By command (the last two recordings together):

| Command | The same |
|---|---|
| 00, 03, 0A, 0B, 11 | all |
| 01 | 99.9% |
| 07 | 99.8% |
| 08 | 91.8% |
| 09 | 94.7% |
| 06 | 99.8% (it follows 09) |

`dsp4_check -s` replays the recordings with the chip's state saved and loaded again after every
transfer: the same results.

## Not exact yet

- **09 on hills.** Where the road climbs above the horizon, the clip line the original uses for
  objects further on is sometimes higher than the walk's highest row (its rule isn't worked out),
  so a few sprites are shown that the original hides. The per-line budget (33 tiles) is exact
  in every recorded case but a few.
- **08's offsets** (segment words 5-8, the game's FF12h/00EEh): the original interpolates them
  per line slightly differently (it behaves like word × the line's distance), one off in some
  lines. And setup words other than the game's (8-15: row limits) are not modelled.
- **Timing.** Answers are immediate; the original takes time over each command, so the game runs
  its frames with more time to spare (it doesn't change what the game does, only how soon).
- Commands the game doesn't use (02, 04, 05, 0C-10, 12-1F) do nothing here; 03's first two words
  are taken as the game sends them.
