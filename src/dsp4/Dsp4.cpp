// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) StackBlender LLC
#include "dsp4/Dsp4.h"

#include <algorithm>

namespace dsp4 {

namespace {

int32_t s16(int32_t v) { return int16_t(uint16_t(v)); }
// x / 2^n, rounded down (also for negative x)
int32_t asr(int64_t x, int n) {
	int64_t d = int64_t(1) << n;
	return int32_t(x >= 0 ? x / d : -((-x + d - 1) / d));
}
// x / d, rounded toward zero
int32_t tz(int64_t x, int64_t d) { return int32_t(x >= 0 ? x / d : -(-x / d)); }

// The value k lines into a segment of n lines between boundary values a and b: steps of
// (b - a) * floor(16384 / n) / 16384 (the chip's reciprocal), plus a starting fraction.
int32_t interp(int32_t a, int32_t b, int32_t n, int32_t k, int32_t bias) {
	return a + asr(int64_t(k) * (b - a) * (16384 / n) + bias, 14);
}

// A nibble of command 11's answer: v / 48 (as v * 1365 / 65536, rounded down), low 4 bits.
int32_t nibble(int32_t v) { return asr(int64_t(s16(v)) * 1365, 16) & 15; }

} // namespace

// Resumable command code: a command runs until it needs an input word (NEED) or has an output
// word (GIVE), and continues there at the next transfer. Its values live in m (State), so a save
// state holds them; the labels are the resume points.
#define NEED(label, var) do { m.pc = label; m.want = 1; return; case label: var = m.in; } while(0)
#define GIVE(label, value) do { m.out = int32_t(uint16_t(value)); m.pc = label; m.want = 0; return; case label:; } while(0)

void Dsp4::reset() {
	m = State{};
	m.dr = 0xffff;
}

void Dsp4::writeData(uint8_t value) {
	if(!m.highByte) {
		m.lowByte = value;
		m.highByte = 1;
		return;
	}
	m.highByte = 0;
	writeWord(uint16_t(m.lowByte | value << 8));
}

uint8_t Dsp4::readData() {
	if(!m.readHigh) {
		m.readValue = readWord();
		m.readHigh = 1;
		return uint8_t(m.readValue);
	}
	m.readHigh = 0;
	return uint8_t(m.readValue >> 8);
}

void Dsp4::writeWord(uint16_t word) {
	if(!m.busy) return start(word);
	if(!m.want) {  // a write where an output is due: the output is skipped
		m.dr = m.out;
		return step();
	}
	m.dr = word;
	m.in = word;
	step();
}

uint16_t Dsp4::readWord() {
	if(!m.busy) return uint16_t(m.dr);
	if(m.want) {  // a read where an input is due: the data register's word is that input
		m.in = m.dr;
		step();
		return uint16_t(m.dr);
	}
	uint16_t value = uint16_t(m.out);
	m.dr = value;
	step();
	return value;
}

void Dsp4::start(uint16_t word) {
	int32_t c = word & 0xff;
	if(c >= 0x20) c = 0x10 | (c & 0x0f);
	switch(c) {
	case 0x00: case 0x01: case 0x03: case 0x06: case 0x07: case 0x08: case 0x09: case 0x0a:
	case 0x0b: case 0x11:
		m.command = c;
		m.busy = 1;
		m.pc = 0;
		step();
		return;
	default:  // not measured (the game doesn't use it): taken as doing nothing
		return;
	}
}

void Dsp4::step() {
	switch(m.command) {
	case 0x00: c00(); break;
	case 0x01: c01(); break;
	case 0x03: c03(); break;
	case 0x06: c06(); break;
	case 0x07: c07(); break;
	case 0x08: c08(); break;
	case 0x09: c09(); break;
	case 0x0a: c0a(); break;
	case 0x0b: c0b(); break;
	case 0x11: c11(); break;
	}
}

// 00: a signed 16 x 16 multiply; the 32-bit product, low word first
void Dsp4::c00() {
	switch(m.pc) {
	case 0:
		NEED(1, m.a);
		NEED(2, m.c);
		m.i = s16(m.a) * s16(m.c);
		GIVE(3, m.i);
		GIVE(4, m.i >> 16);
	}
	m.busy = 0;
}

// 0A: four values, each between a nibble of b and the same nibble of c (signed, times 48), at
// fraction a / 32768; the high nibbles first. Its last two answers stay behind as the starting
// fractions of the next road command's first segment.
void Dsp4::c0a() {
	switch(m.pc) {
	case 0:
		NEED(1, m.a);
		NEED(2, m.st[0]);
		NEED(3, m.c);
		for(int i = 0; i < 4; i++) {
			int shift = 4 * (3 - i);
			int32_t x = (m.st[0] >> shift) & 15, y = (m.c >> shift) & 15;
			if(x >= 8) x -= 16;
			if(y >= 8) y -= 16;
			m.outs[i] = x * 48 + asr(int64_t(y - x) * 48 * s16(m.a), 15);
		}
		m.bx = (m.outs[2] & 0xffff) >> 2;
		m.bw = (m.outs[3] & 0xffff) >> 2;
		for(m.k = 0; m.k < 4; m.k++) GIVE(4, m.outs[m.k]);
	}
	m.busy = 0;
}

// 11: four values, each / 48 (rounded down), their low 4 bits packed, the first highest
void Dsp4::c11() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 4; m.k++) NEED(1, m.st[m.k]);
		GIVE(2, nibble(m.st[0]) << 12 | nibble(m.st[1]) << 8 | nibble(m.st[2]) << 4 | nibble(m.st[3]));
	}
	m.busy = 0;
}

// 01: the road, one track. 22 setup words, then segments (a distance and three slope words),
// each answered by a header and its screen lines (3 words each). docs/commands.md.
void Dsp4::c01() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 22; m.k++) NEED(1, m.st[m.k]);
		m.prevRow = m.top = s16(m.st[2]);
		m.lv = m.st[9];
		m.wPrev = s16(m.st[7]);
		m.uPrev = 0;
		// the row and width multipliers and their slopes, 16.16
		m.S1 = int32_t(uint32_t(int64_t(s16(m.st[1])) * 65536 + s16(m.st[16]) * 256 + m.st[0]));
		m.g1 = int32_t(uint32_t(int64_t(s16(m.st[12])) * 65536 + s16(m.st[19]) * 256 + m.st[11]));
		m.S7 = int32_t(uint32_t(int64_t(s16(m.st[7]) + s16(m.st[18])) * 65536 + m.st[6] + m.st[17]));
		m.g7 = int32_t(uint32_t(int64_t(s16(m.st[14])) * 65536 + s16(m.st[20]) * 256 + m.st[13]));
		m.first = 1;
		for(;;) {
			if(m.first) {
				m.D = m.st[15];
				m.e1 = m.e2 = 0;
				m.e3 = s16(m.st[21]);
			} else {
				NEED(2, m.D);
				if(m.D == 0x8000) break;
				NEED(3, m.e1);
				NEED(4, m.e2);
				NEED(5, m.e3);
				m.e1 = s16(m.e1); m.e2 = s16(m.e2); m.e3 = s16(m.e3);
			}
			m.m1 = m.S1 >> 16;
			m.m7 = m.S7 >> 16;
			m.rowA = s16(asr(int64_t(m.D) * m.m1, 15));
			m.rowB = s16(asr(int64_t(m.D) * m.m7, 15));
			m.nf = m.prevRow - m.rowA;
			m.U = asr(int64_t(s16(m.st[10])) * m.D, 15) - s16(m.st[10]);
			m.n = std::max(0, std::min(m.prevRow, m.top) - m.rowA);
			GIVE(6, m.m7);
			GIVE(7, m.rowB);
			GIVE(8, m.m1);
			GIVE(9, m.rowA);
			GIVE(10, m.n);
			for(m.row = std::min(m.prevRow, m.top); m.row > m.rowA; m.row--) {
				m.k = m.prevRow - m.row;
				GIVE(11, m.lv);
				m.lv -= 4;
				GIVE(12, s16(m.st[4]) - s16(m.st[5]) + m.e3 + (s16(m.st[2]) - m.row) + interp(m.uPrev, m.U, m.nf, m.k, m.bx));
				GIVE(13, s16(m.st[8]) + interp(m.wPrev, m.rowB, m.nf, m.k, m.bw));
			}
			m.prevRow = m.rowA;
			m.top = std::min(m.top, m.rowA);
			m.wPrev = m.rowB;
			m.uPrev = m.U;
			m.bw = m.bx = 0x2000;
			m.first = 0;
			m.g1 = int32_t(uint32_t(m.g1) + uint32_t(m.e1 * 256));
			m.g7 = int32_t(uint32_t(m.g7) + uint32_t(m.e2 * 256));
			m.S1 = int32_t(uint32_t(m.S1) + uint32_t(m.g1));
			m.S7 = int32_t(uint32_t(m.S7) + uint32_t(m.g7));
		}
	}
	m.busy = 0;
}

// 07: the road with its rows given: 17 setup words (the first segment's six at 11-16), then
// segments of six words (distance, row and its slope, width row and its slope, x offset).
void Dsp4::c07() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 17; m.k++) NEED(1, m.st[m.k]);
		m.prevRow = m.top = s16(m.st[2]);
		m.lv = m.st[9];
		m.wPrev = s16(m.st[7]);
		m.uPrev = 0;
		m.first = 1;
		for(;;) {
			if(m.first) {
				for(int i = 0; i < 6; i++) m.inp[i] = m.st[11 + i];
			} else {
				NEED(2, m.inp[0]);
				if(m.inp[0] == 0x8000) break;
				for(m.k = 1; m.k < 6; m.k++) NEED(3, m.inp[m.k]);
			}
			m.D = m.inp[0];
			m.rowA = s16(m.inp[1]) + asr(int64_t(s16(m.inp[2])) * m.D, 15);
			m.rowB = s16(m.inp[3]) + asr(int64_t(s16(m.inp[4])) * m.D, 15);
			m.e3 = s16(m.inp[5]);
			m.nf = m.prevRow - m.rowA;
			m.U = asr(int64_t(s16(m.st[10])) * m.D, 15) - s16(m.st[10]);
			m.n = std::max(0, std::min(m.prevRow, m.top) - m.rowA);
			GIVE(4, m.rowB);
			GIVE(5, m.rowA);
			GIVE(6, m.n);
			for(m.row = std::min(m.prevRow, m.top); m.row > m.rowA; m.row--) {
				m.k = m.prevRow - m.row;
				GIVE(7, m.lv);
				m.lv -= 4;
				GIVE(8, s16(m.st[4]) - s16(m.st[5]) + m.e3 + (s16(m.st[2]) - m.row) + interp(m.uPrev, m.U, m.nf, m.k, m.bx));
				GIVE(9, s16(m.st[8]) + interp(m.wPrev, m.rowB, m.nf, m.k, m.bw));
			}
			m.prevRow = m.rowA;
			m.top = std::min(m.top, m.rowA);
			m.wPrev = m.rowB;
			m.uPrev = m.U;
			m.bw = m.bx = 0x2000;
			m.first = 0;
		}
	}
	m.busy = 0;
}

// 08: two tracks, four bytes a line (two a track): 45 setup words, then segments of nine words.
// docs/commands.md (measured with the game's setup words; others are not modelled).
void Dsp4::c08() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 45; m.k++) NEED(1, m.st[m.k]);
		m.W = s16(m.st[37]);
		GIVE(2, ((m.st[17] - m.W) & 255) << 8 | ((m.st[16] - m.W) & 255));
		for(int s = 0; s < 2; s++) {
			m.sPrev[s] = m.sTop[s] = s16(m.st[24 + 2 * s]);
			m.sLv[s] = m.st[20 + 2 * s];
		}
		for(int q = 0; q < 4; q++) m.E[q] = asr(int64_t(s16(m.st[41 + q])) * m.st[36], 15);
		for(;;) {
			NEED(3, m.inp[0]);
			if(m.inp[0] == 0x8000) {
				m.bw = m.bx = 0;
				GIVE(4, 0);
				break;
			}
			for(m.k = 1; m.k < 9; m.k++) NEED(5, m.inp[m.k]);
			m.D = m.inp[0];
			m.b = s16(m.inp[3]);
			for(int q = 0; q < 4; q++) m.E1[q] = asr(int64_t(s16(m.inp[5 + q])) * m.D, 15);
			for(m.side = 0; m.side < 2; m.side++) {
				m.rowA = s16(m.inp[2 + 2 * m.side]);
				m.nf = m.sPrev[m.side] - m.rowA;
				m.n = std::max(0, std::min(m.sPrev[m.side], m.sTop[m.side]) - m.rowA);
				GIVE(6, m.n);
				for(m.row = std::min(m.sPrev[m.side], m.sTop[m.side]); m.row > m.rowA; m.row--) {
					GIVE(7, m.sLv[m.side]);
					m.sLv[m.side] -= 4;
					{
						int32_t k = m.sPrev[m.side] - m.row + 1, r = 16384 / m.nf;
						int32_t v = m.W + 1 + tz(int64_t(k) * (m.b - m.W) * r, 16384);
						int32_t byte[2];
						for(int j = 0; j < 2; j++) {
							int q = 2 * m.side + j;
							int32_t e = m.E[q] + asr(int64_t(k) * (m.E1[q] - m.E[q]) * r, 14);
							byte[j] = std::max(m.st[4 + q], std::min(m.st[q], m.st[16 + q] + 1 - v + e));
						}
						m.a = (byte[1] & 255) << 8 | (byte[0] & 255);
					}
					GIVE(8, m.a);
				}
				m.sPrev[m.side] = m.rowA;
				m.sTop[m.side] = std::min(m.sTop[m.side], m.rowA);
			}
			m.W = m.b;
			for(int q = 0; q < 4; q++) m.E[q] = m.E1[q];
		}
	}
	m.busy = 0;
}

void Dsp4::addSprite(int32_t x, bool big) {
	if(m.spriteCount < 128) {
		int32_t bits = (x & 0x100 ? 1 : 0) | (big ? 2 : 0);
		m.hiTable[m.spriteCount / 8] |= bits << (2 * (m.spriteCount % 8));
	}
	m.spriteCount++;
}

// 03: starts the frame's sprites (the high table and the line budget) with one sprite: two
// words not measured, then x, y and the tile word
void Dsp4::c03() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 5; m.k++) NEED(1, m.st[m.k]);
		m.spriteCount = 0;
		std::fill(std::begin(m.hiTable), std::end(m.hiTable), 0);
		std::fill(std::begin(m.lines), std::end(m.lines), 0);
		m.bw = m.bx = 0x800;
		addSprite(m.st[2], false);
		GIVE(2, 1);
		GIVE(3, (m.st[3] & 255) << 8 | (m.st[2] & 255));
		GIVE(4, m.st[4]);
	}
	m.busy = 0;
}

// 0B: one more sprite: x, y, the tile word
void Dsp4::c0b() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 3; m.k++) NEED(1, m.st[m.k]);
		addSprite(m.st[0], false);
		GIVE(2, 1);
		GIVE(3, (m.st[1] & 255) << 8 | (m.st[0] & 255));
		GIVE(4, m.st[2]);
	}
	m.busy = 0;
}

// 06: the high sprite table, 16 words
void Dsp4::c06() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 16; m.k++) GIVE(1, m.hiTable[m.k]);
	}
	m.busy = 0;
}

// One entry of 09: the sprite at (x, y), 16 pixels (big) or 8; if it reaches below the clip line,
// a masking sprite there first. Hidden if it is wholly below the line or off the screen's sides,
// or if a line it covers already holds 33 tiles. Answers: 1 and two OAM words a sprite, then 0.
void Dsp4::entry(int32_t x, int32_t y, int32_t clip, bool big, int32_t attr, int32_t tile) {
	int32_t h = big ? 16 : 8;
	m.olen = 0;
	if(y <= clip && x + h > 0 && x <= 256) {
		int32_t sy[2], hi[2], count = 0;
		if(y + h > clip) sy[count] = clip, hi[count++] = 0x00ee;
		sy[count] = y, hi[count++] = (attr + tile) & 0xffff;
		auto line = [&](int32_t at) -> int32_t* {
			int32_t i = at + LineBase;
			return i >= 0 && i < LineSpan ? &m.lines[i] : nullptr;
		};
		bool fits = true;
		for(int s = 0; s < count && fits; s++)
			for(int32_t l = sy[s]; l < sy[s] + h; l++)
				if(int32_t* c = line(l); c && *c + h / 8 > 33) fits = false;
		if(fits) {
			for(int s = 0; s < count; s++) {
				for(int32_t l = sy[s]; l < sy[s] + h; l++)
					if(int32_t* c = line(l)) *c += h / 8;
				addSprite(x, big);
				m.obuf[m.olen++] = 1;
				m.obuf[m.olen++] = (sy[s] & 255) << 8 | (x & 255);
				m.obuf[m.olen++] = hi[s];
			}
		}
	}
	m.obuf[m.olen++] = 0;
}

// 09: the sprites. 7 setup words, then a stream: walks along the road (rows; 0000 advances a
// segment), view blocks (a word with the top bit set and 7 more: answered by one word), objects
// on the road (distance, rows, x, z, attribute) or fixed (an attribute 1000h-1FFFh), each with
// groups of entries (tile, dy, dx). 8000h ends it. docs/commands.md.
void Dsp4::c09() {
	switch(m.pc) {
	case 0:
		for(m.k = 0; m.k < 7; m.k++) NEED(1, m.st[m.k]);
		m.X = m.Y = m.clip = m.attr = m.group = 0;
		m.vX = m.vY = m.vClip = 0;
		m.gmin = 0x7fff;
		m.n = 0;  // set when the stream ends
		NEED(2, m.w1);
		m.mode = 0;
		for(;;) {
			if(m.mode == 1) {  // after an entry or a header: the next entry, or 0 to end the group
				NEED(3, m.t);
				if(m.t != 0) {
					NEED(4, m.dy);
					NEED(5, m.dx);
					entry(m.X + s16(m.dx), m.Y + s16(m.dy), m.clip, m.group == 0, m.attr, m.t);
					for(m.opos = 0; m.opos < m.olen; m.opos++) GIVE(6, m.obuf[m.opos]);
					continue;
				}
				NEED(7, m.w1);
				if(m.group == 0 && m.w1 == 0) {  // group 0 ended; an empty group 1
					m.group = 1;
					NEED(8, m.w1);
				}
			}
			if(m.w1 >= 0x2000 && m.w1 < 0x8000) {  // a new group, starting with this entry
				m.group++;
				NEED(9, m.dy);
				NEED(10, m.dx);
				entry(m.X + s16(m.dx), m.Y + s16(m.dy), m.clip, m.group == 0, m.attr, m.w1);
				for(m.opos = 0; m.opos < m.olen; m.opos++) GIVE(11, m.obuf[m.opos]);
				m.mode = 1;
				continue;
			}
			if(m.w1 >= 0x1000 && m.w1 < 0x2000) {  // an object fixed on the screen (the view's place)
				m.attr = m.w1;
				m.group = 0;
				m.X = m.vX;
				m.Y = m.vY;
				m.clip = m.vClip;
				m.mode = 1;
				continue;
			}
			if(m.w1 == 0x8000) break;
			m.row = s16(m.w1);  // a walk: rows, 0000 between segments, then a distance or a view
			m.gmin = std::min(m.gmin, m.row);
			for(;;) {
				NEED(12, m.t);
				if(m.t == 0) {
					NEED(13, m.t);
					if(m.t == 0x8000) { m.n = 1; break; }
					m.row = s16(m.t);
					m.gmin = std::min(m.gmin, m.row);
					continue;
				}
				if(m.t == 0x8000) { m.n = 1; break; }
				if(m.t & 0x8000) {  // a view block
					m.inp[0] = m.t;
					for(m.k = 1; m.k < 8; m.k++) NEED(14, m.inp[m.k]);
					{
						int32_t p2 = m.inp[1], p4 = s16(m.inp[3]), p5 = s16(m.inp[4]), p6 = s16(m.inp[5]);
						int32_t p7 = m.inp[6], p8 = s16(m.inp[7]);
						m.a = p8 - (p6 + asr(int64_t(p5 - p6) * p2, 16));
						m.vX = m.st[0] + asr(int64_t(m.a) * p7, 15);
						m.vY = m.st[1] + asr(int64_t(p4) * p7, 15);
						m.vClip = m.st[1] + m.row;
					}
					GIVE(15, m.a);
					m.mode = 1;
					break;
				}
				m.D = m.t;  // an object on the road
				NEED(16, m.rowB);
				NEED(17, m.rowA);
				NEED(18, m.a);
				NEED(19, m.c);
				NEED(20, m.attr);
				m.X = m.st[0] + asr(int64_t(s16(m.a)) * m.D, 15) - s16(m.rowB);
				m.Y = m.st[1] + s16(m.rowA) + asr(int64_t(s16(m.c)) * m.D, 15);
				m.clip = m.st[1] + std::min(m.gmin, s16(m.rowA));
				m.group = 0;
				m.mode = 1;
				break;
			}
			if(m.n) break;
		}
	}
	m.busy = 0;
}

#undef NEED
#undef GIVE

std::vector<uint8_t> Dsp4::saveState() const {
	static_assert(sizeof(State) % 4 == 0, "State holds 32-bit integers only");
	const int32_t* v = reinterpret_cast<const int32_t*>(&m);
	size_t n = sizeof(State) / 4;
	std::vector<uint8_t> s = {'D', 'S', 'P', '4', 1};
	for(size_t i = 0; i < n; i++)
		for(int b = 0; b < 4; b++) s.push_back(uint8_t(uint32_t(v[i]) >> (8 * b)));
	return s;
}

bool Dsp4::loadState(const uint8_t* data, size_t size) {
	size_t n = sizeof(State) / 4;
	if(size != 5 + 4 * n || data[0] != 'D' || data[1] != 'S' || data[2] != 'P' || data[3] != '4' || data[4] != 1)
		return false;
	State s{};
	int32_t* v = reinterpret_cast<int32_t*>(&s);
	for(size_t i = 0; i < n; i++) {
		const uint8_t* p = data + 5 + 4 * i;
		v[i] = int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24);
	}
	if(s.pc < 0 || s.pc > 20 || s.olen < 0 || s.olen > 8 || s.opos < 0 || s.opos > 8 || s.side < 0 || s.side > 2)
		return false;
	m = s;
	return true;
}

} // namespace dsp4
