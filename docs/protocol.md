# The DSP-4's protocol, as observed

What Top Gear 3000 sends the DSP-4 and what the chip answers, worked out by recording the
traffic with the original chip running in an emulator (docs/clean-room.md). Word counts and
structure only; what each value means is worked out separately.

## Transfers

All transfers are 16-bit words through the data register, low byte first. A command starts with
a command word; only its low byte counts, and commands 20h-FFh act as 10h-1Fh. Each command
then takes input words and gives output words. An output the game doesn't want can be skipped by
writing instead of reading: the chip takes the write as the read (and the written value is lost).

## Commands seen in a race

Each update of the picture (about 3 in every 7 frames) runs 01, 07, 08, 03, 0B and 09 once each,
with multiplies (00) and 0A calls in between.

| Command | In | Out | Seen as |
|---|---|---|---|
| 00 | 2 | 2 | signed 16 x 16 multiply, 32-bit product, low word first |
| 03 | 5 | 3 | one sprite entry: the outputs pack two inputs into one word |
| 0A | 3 | 4 | |
| 0B | 3 | 3 | as 03 |
| 11 | 4 | 1 | (before the race) |
| 01 | streamed | streamed | the road, one track |
| 07 | streamed | streamed | the road, a variant |
| 08 | streamed | streamed | the road, two tracks |
| 09 | streamed | streamed | sprites placed along the road |

### 01: the road

22 setup words, then the first segment's answer, then for each further segment its distance and
3 more words and its answer. A distance of 8000h ends the command, with no answer.

A segment's answer: a 5-word header, whose last word is n, then n lines of 3 words each. n is the
number of screen lines from the previous segment's row down to this one's (the first segment's
from the setup's start row). Within a segment, the first word of each line falls by 4 a line.

### 07: the road, a variant

As 01, with 17 setup words, segments of a distance and 5 more words, and a 3-word header.

### 08: the road, two tracks

45 setup words, answered by one word. Then for each segment: its distance and 8 more words,
answered for each of the two tracks by n and then n lines of 2 words. A distance of 8000h ends
it, answered by one word.

### 09: sprites

18 setup words, answered by one word ("go on", 0000). Then a stream of objects, each of sprite
entries. An entry is 3 words (a tile and attribute word of 2000h or more, an x offset, a y
offset); it is answered 0001 and two sprite-table words and then 0000, or 0000 alone if it isn't
shown. Between entries:

- 0000 and then a word of 2000h or more: a new group of entries in the same object.
- 0000 and then a word from 1000h to 1FFFh: an object at a fixed place on screen; its entries
  follow (after one 0000 for some objects).
- 0000 and then a small word: an object on the road. Rows follow, each followed by 0000, until a
  distance comes instead of the 0000; then the segment's two rows, the object's x, 0000 and an
  attribute word; then its entries (after one 0000 for some objects).
- Where a distance comes with its top bit set: 8000h ends the command (one more word, answered
  by 16 words); any other value starts a 9-word view block like the end of the setup, answered
  by one word.
