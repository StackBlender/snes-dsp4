// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) StackBlender LLC
//
// A clean-room DSP-4: the coprocessor in Top Gear 3000 (an NEC uPD77C25 running the console
// maker's program), reimplemented from black-box measurement of what the chip answers, never from
// its program. No public description of its commands exists; docs/commands.md has what was
// measured.
//
// The SNES sees two registers: data and status. Every transfer is a 16-bit word, low byte first.
// A command is a word (its low byte; 20h-FFh act as 10h-1Fh), then input and output words in the
// order the command needs them. As on the chip, a read where an input is due gives the data
// register's word to the command as that input, and a write where an output is due takes the
// output's place (the written word is lost).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsp4 {

class Dsp4 {
public:
	Dsp4() { reset(); }
	void reset();

	uint8_t readData();
	void writeData(uint8_t value);
	// Status: ready (bit 7) always; bit 4 while a word is half transferred.
	uint8_t readStatus() const { return uint8_t(0x80 | ((m.highByte || m.readHigh) ? 0x10 : 0x00)); }

	// Word transfers (what two byte transfers make).
	uint16_t readWord();
	void writeWord(uint16_t word);

	// The chip's whole state, for a host's save states: a versioned blob, the same on every
	// platform. loadState returns false (and changes nothing) if the blob isn't one this class
	// wrote.
	std::vector<uint8_t> saveState() const;
	bool loadState(const uint8_t* data, size_t size);

	// Lines the per-line sprite budget covers (screen lines -384 to 639).
	static constexpr int LineSpan = 1024, LineBase = 384;

	// Everything the chip keeps, as 32-bit integers only (so a save state is just their values).
	struct State {
		// transfers
		int32_t busy, want, out, in, dr, pc, command, highByte, lowByte, readHigh, readValue;
		// what the first road segment's interpolation starts from (fractions, 1/16384), left by
		// the commands before
		int32_t bw, bx;
		// the sprites of the frame (since 03): the high table (2 bits a sprite) and the sprite
		// tiles each screen line holds
		int32_t spriteCount, hiTable[16], lines[LineSpan];
		// the running command's own values
		int32_t st[45];
		int32_t i, k, n, row, D, e1, e2, e3, m1, m7, rowA, rowB, nf, U, prevRow, top, lv, wPrev, uPrev, first;
		int32_t S1, g1, S7, g7;
		int32_t W, b, side, E[4], E1[4], sPrev[2], sTop[2], sLv[2], inp[9];
		int32_t X, Y, clip, attr, group, gmin, vX, vY, vClip, w1, t, dy, dx, mode, obuf[8], olen, opos;
		int32_t a, c, outs[4];
	};

private:
	void start(uint16_t command);
	void step();
	void c00(); void c01(); void c03(); void c06(); void c07(); void c08(); void c09(); void c0a();
	void c0b(); void c11();
	void entry(int32_t x, int32_t y, int32_t clip, bool big, int32_t attr, int32_t tile);
	void addSprite(int32_t x, bool big);

	State m{};
};

} // namespace dsp4
