// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) StackBlender LLC
// Saving and loading the chip's state mid-command changes nothing; and some commands' known
// answers.
#include "dsp4/Dsp4.h"

#include <cstdio>
#include <vector>

static int failures = 0;
static void expect(bool ok, const char* what) {
	if(!ok) std::printf("FAIL %s\n", what), failures++;
}

static void put(dsp4::Dsp4& chip, uint16_t w) { chip.writeData(uint8_t(w)); chip.writeData(uint8_t(w >> 8)); }
static uint16_t get(dsp4::Dsp4& chip) { uint16_t lo = chip.readData(); return uint16_t(lo | chip.readData() << 8); }

int main() {
	using dsp4::Dsp4;
	{
		Dsp4 chip;
		put(chip, 0x00); put(chip, 3); put(chip, 0xfffb);
		expect(get(chip) == 0xfff1 && get(chip) == 0xffff, "00: 3 x -5");
		put(chip, 0x11); put(chip, 0xff00); put(chip, 0x0064); put(chip, 0xffce); put(chip, 0x003c);
		expect(get(chip) == 0xa2e1, "11: four values / 48, a nibble each");
		put(chip, 0x0a); put(chip, 0x4000); put(chip, 0x0000); put(chip, 0x1000);
		expect(get(chip) == 24 && get(chip) == 0 && get(chip) == 0 && get(chip) == 0, "0A: halfway to 48");
		put(chip, 0x03); put(chip, 5); put(chip, 0x0b); put(chip, 0x40); put(chip, 0x20); put(chip, 0x1000);
		expect(get(chip) == 1 && get(chip) == 0x2040 && get(chip) == 0x1000, "03: the first sprite");
		put(chip, 0x0b);
		put(chip, 0x1c8); put(chip, 0x30); put(chip, 0x1200);
		expect(get(chip) == 1 && get(chip) == 0x30c8 && get(chip) == 0x1200, "0B: a sprite");
		put(chip, 0x06);
		std::vector<uint16_t> table;
		for(int i = 0; i < 16; i++) table.push_back(get(chip));
		expect(table[0] == 0x0004, "06: the high table (the second sprite's x is over 255)");
		expect(chip.readStatus() == 0x80, "status");
	}
	{
		// A road (01) and sprites (09), with a save and load after every transfer: the same answers.
		std::vector<std::pair<bool, uint16_t>> stream;  // write?, word
		auto w = [&](std::vector<uint16_t> v) { for(auto x : v) stream.push_back({true, x}); };
		auto r = [&](int n) { for(int i = 0; i < n; i++) stream.push_back({false, 0}); };
		w({0x01, 0, 0x30, 0x30, 0xff00, 0x100, 0x110, 0, 0x40, 0x100, 0x500, 0x80, 0, 0, 0, 0, 0x6000,
		   0, 0, 0, 0, 0, 0});
		r(5 + 12 * 3);
		w({0x3000, 0, 0, 0}); r(5 + 18 * 3);
		w({0x8000});
		w({0x03, 5, 0x0b, 0x40, 0x20, 0x1000}); r(3);
		w({0x09, 0x0080, 0x00a0, 0, 0, 0x0100, 0x0004, 0x00c0, 0x0020, 0x9000, 0x0100, 0x0030, 0x0030, 0, 0,
		   0x6000, 0});
		r(1);
		w({0x0000, 0x1e00, 0x2000, 0xfff0, 0xffe0}); r(4);
		w({0x0000, 0x0010, 0x3000, 0x0018, 0x0010, 0x0100, 0x0000, 0x1800, 0x2010, 0xffe8, 0xfffb}); r(4);
		w({0x0000, 0x0000, 0x8000, 0x06}); r(16);
		Dsp4 straight, saved;
		std::vector<uint16_t> a, b;
		for(auto [write, word] : stream) {
			if(write) put(straight, word), put(saved, word);
			else a.push_back(get(straight)), b.push_back(get(saved));
			auto state = saved.saveState();
			Dsp4 fresh;
			expect(fresh.loadState(state.data(), state.size()), "load a saved state");
			saved = fresh;
		}
		expect(a == b, "saving and loading each transfer changes nothing");
		expect(a[0] == 0x40 && a[3] == 0x24 && a[4] == 12 && a[5] == 0x0500 && a[6] == 0xfff0 && a[7] == 0x0140, "01: the first segment");
		Dsp4 chip;
		uint8_t junk[3] = {1, 2, 3};
		expect(!chip.loadState(junk, 3), "a blob that isn't a state is refused");
	}
	std::printf("%s\n", failures ? "dsp4: FAILED" : "dsp4: ok");
	return failures ? 1 : 0;
}
