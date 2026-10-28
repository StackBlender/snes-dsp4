# snes-dsp4

A free, clean-room implementation of the DSP-4, the NEC maths coprocessor in Top Gear 3000,
under the GPL. It will answer the chip's commands with the same results as the original, so
emulators can run the game without the original chip's program, which is the console
maker's code.

**Status:** just started; nothing to use yet.

Made by [StackBlender](https://stackblender.com) for Super Apollo, an emulator that plays
SNES games fast with their music at normal speed. Its other chips are published the same
way: [snes-dsp1](https://github.com/StackBlender/snes-dsp1),
[snes-cx4](https://github.com/StackBlender/snes-cx4).

## How it is made

Clean room: written from public descriptions of what the chip does, never from the original
program's bytes or a disassembly of it, which nobody on this project reads. It is checked
against the original running in the [ares](https://github.com/ares-emulator/ares) emulator,
answer for answer; the tests take the original from the emulator at run time, and it is
never in this repository. The rules: [docs/clean-room.md](docs/clean-room.md).

## License

GNU General Public License, version 3 or (at your option) any later version; see
[LICENSE](LICENSE). For other terms, contact [StackBlender](https://stackblender.com).
