# The DSP-4's commands, as measured

How each command behaves, worked out by putting questions to the original chip (running in an
emulator; docs/clean-room.md) and by recording Top Gear 3000's traffic with it. No public
description of the DSP-4's commands exists.

Every transfer is a 16-bit word, low byte first. A command is a word whose low byte selects it
(20h-FFh act as 10h-1Fh). Values are signed 16-bit unless said otherwise; `x >> n` rounds down.
As on the chip, a write where an output is due takes the output's place (the written word is
lost), and a read where an input is due gives the command the data register's word.

## Arithmetic

| Command | Inputs | Outputs |
|---|---|---|
| 00 | a, b | a × b, 32 bits, low word first |
| 0A | a, b, c | for each nibble of b and c (signed, the high nibble first), with x = 48 × the nibble of b and y = 48 × the nibble of c: x + ((y - x) × a >> 15) |
| 11 | a, b, c, d | one word: of each input v / 48 (as v × 1365 >> 16), the low 4 bits, a's in the high nibble |

## The road: 01, 07, 08

The road is drawn as screen lines from a start row upwards. It is cut into segments, each ending
at a distance D (32768 = the bottom of the screen). A segment's end row is rowA, its width
value rowB; a segment covers the lines from the previous end row up to (not including) its own.
Lines hidden behind a crest (higher than the highest row drawn so far) are not output.

Within a segment of n lines, a value going from a (at its start) to b (at its end) is, k lines
in: a + ((k × (b − a) × floor(16384 / n) + f) >> 14). f is 2000h (a half), except in a road
command's first segment, which starts from whatever the commands before left: 0 after power-on,
08 and 0A (0A: its third and fourth answers ÷ 4, for x and the width), 800h after 03, 2000h after
01 and 07; the others keep it.

**01** (one track): 22 setup words: s0-s21. Then the first segment's answer, then for each
further segment: D, e1, e2, e3, and its answer. D = 8000h ends the command (no answer).

- Two multipliers in 16.16: M1 = s1 + s16/256 + s0/65536, M7 = s7 + s18 + (s6 + s17)/65536;
  their slopes G1 = s12 + s19/256 + s11/65536, G7 = s14 + s20/256 + s13/65536. After each
  segment, G1 += e1/256, G7 += e2/256, M1 += G1, M7 += G7.
- A segment at distance D (the first: s15): rowA = D × int(M1) >> 15, rowB = D × int(M7) >> 15.
  Its answer: int(M7), rowB, int(M1), rowA, n (the lines that follow), then 3 words a line: a
  line value (s9, less 4 each line), x and w.
- x = s4 − s5 + e3 + (s2 − row) + the interpolation of U between segments, U = (s10 × D >> 15)
  − s10 (0 at the start); e3 is the segment's own offset (the first segment's: s21).
- w = s8 + the interpolation of rowB between segments (s7 at the start).
- s2 is the start row (lines are counted from it), s3 a clip row (not modelled: the game sends a
  negative one).

**07** (rows given): 17 setup words; the first segment's six are s11-s16. Each further segment:
D, rA, hA, rB, hB, e3, with rowA = rA + (hA × D >> 15) and rowB = rB + (hB × D >> 15). Its answer
is rowB, rowA, n, then the lines as for 01 (x from s10 and e3, w from rowB).

**08** (two tracks, four bytes a line): 45 setup words, answered by one word; then segments of
nine words: D, then rowA for each track (words 2 and 4), the width row (word 3) and an offset for
each of the four bytes (words 5-8). D = 8000h ends it (answered by 0). Each track's answer: n,
then two words a line: a line value (s20 or s22, less 4 each line) and two bytes. Byte q (0-3)
of a line k lines into the segment: clamp(s(16+q) + 1 − V + E, s(4+q), s(q)), where V = W + 1
+ ((k + 1) × (rowB − W) × floor(16384 / n) / 16384, rounded toward zero) runs from the previous
width row W (s37 at the start), and E = Eprev + ((k + 1) × (E1 − Eprev) × floor(16384 / n) >>
14) with E1 = word (5+q) × D >> 15 (at the start: s(41+q) × s36 >> 15). Start rows: s24 and s26.
Measured with the game's setup words; s8-s15 act as row limits (other values are not modelled).

## Sprites: 03, 0B, 09, 06

**03** starts the frame's sprites (the high table, the line budget below) with one sprite: two
words (the game sends 5 and 0Bh; not measured further), x, y, the tile word. Answer: 1,
y << 8 | x (8 bits each), the tile word. **0B** adds one sprite: x, y, the tile word; the same
answer. **06** answers the high table: 16 words, 2 bits for each sprite output since 03 (bit 0:
x bit 8; bit 1: 16 pixels).

**09** places objects' sprites. 7 setup words (s0 = the screen's x centre, s1 its road row 0),
then a stream:

- A walk along the road: a row, then for each further segment 0000 and its row. It ends with
  either a distance (an object on the road follows) or a word with the top bit set (a view
  block); 8000h ends the command.
- A view block: the marker and 7 words p2-p8. Answer: p8 − floor(p6 + (p5 − p6) × p2 / 65536).
  It places fixed objects at x = s0 + (answer × p7 >> 15), y = s1 + (p4 × p7 >> 15), clipped
  at s1 + the walk's last row.
- An object on the road: D, rowB, rowA, x, z, attribute: at x = s0 + (x × D >> 15) − rowB,
  y = s1 + rowA + (z × D >> 15), clipped at s1 + the highest row of the walk so far.
- An object fixed on the screen: an attribute 1000h-1FFFh (at the last view's place).
- After an object, its entries: tile word, dy, dx. 0000 ends a group; a tile word of 2000h-7FFFh
  after it starts the next group (and is its first entry). Group 0's sprites are 16 pixels,
  later groups' 8. Right after group 0, a second 0000 means an empty group 1. After the entries,
  0000 then a row starts the next walk.

An entry's sprite is at (X + dx, Y + dy), its tile word the attribute + the entry's tile word. It
is hidden if it is wholly below the clip line, off the screen's sides (x + size ≤ 0 or x > 256),
or if a screen line it covers already holds 33 tiles (a 16-pixel sprite counts 2). If it reaches
below the clip line, a masking sprite (tile word 00EEh) is put on the clip line first. Answer:
for each sprite 1, y << 8 | x, the tile word; then 0.
