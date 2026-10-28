# snes-dsp4

A free, clean-room implementation of the DSP-4, the NEC maths coprocessor in Top Gear 3000,
under the GPL. It will answer the chip's commands with the same results as the original, so
emulators can run the game without the original chip's program, which is the console
maker's code.

**Status:** Top Gear 3000 plays with it. In three recordings of the game with the original
chip, 96-100% of the reads are the same (all of them in a race on flat road); what differs is
listed in [docs/exactness.md](docs/exactness.md). The commands:
[docs/commands.md](docs/commands.md).

## Using it

`src/dsp4` is plain C++17 with no dependencies. `dsp4::Dsp4` has the chip's two registers
(`readData`, `writeData`, `readStatus`; or word transfers) and `saveState` / `loadState`. Map the
data and status registers where the cartridge has them (as for the DSP-1).
`tests/state_test.cpp` runs with `ctest`; `dsp4_check` compares recordings of the original's
traffic (kept outside the repository).

Made by [StackBlender](https://stackblender.com) for Super Apollo, an emulator that plays
SNES games fast with their music at normal speed. Its other chips are published the same
way: [snes-dsp1](https://github.com/StackBlender/snes-dsp1),
[snes-dsp2](https://github.com/StackBlender/snes-dsp2),
[snes-dsp3](https://github.com/StackBlender/snes-dsp3),
[snes-st010](https://github.com/StackBlender/snes-st010),
[snes-cx4](https://github.com/StackBlender/snes-cx4).

## How it is made

Clean room: written from measuring what the original answers (no public description of its
commands exists), never from the original program's bytes or a disassembly of it, which nobody
on this project reads. It is checked
against the original running in the [ares](https://github.com/ares-emulator/ares) emulator,
answer for answer; the tests take the original from the emulator at run time, and it is
never in this repository. The rules: [docs/clean-room.md](docs/clean-room.md).

## License

GNU General Public License, version 3 or (at your option) any later version; see
[LICENSE](LICENSE). For other terms, contact [StackBlender](https://stackblender.com).
