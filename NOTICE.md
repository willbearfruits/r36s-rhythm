# Notices

RHYTHM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE).

Copyright (C) 2026 Yaniv Schonfeld (willbearfruits).

## What it carries besides its own code

- **Terminus Font**, the text on screen: `core/font_data.c` holds its bitmaps at eight sizes, rendered from the font
  files by `tools/mkfont.py`; `docs/mockups/sheet.py` draws with the same fonts. Copyright (C) 2020 Dimitar Toshkov
  Zhekov, with Reserved Font Name "Terminus Font". SIL Open Font License 1.1:
  [LICENSES/Terminus-Font-OFL-1.1.txt](LICENSES/Terminus-Font-OFL-1.1.txt).

That is all. There are no recorded samples in it: the bundled break, `BUNDLED/BREAKS/DUSTY_94.WAV`, is made by
RHYTHM's own engine (`build/host/render break`, which `make run` and `make port` call), from its Lo-Fi Beat kit at
94 BPM.

## From the maker's other work

Same author, same licence; named so the history is clear.

- **thump**, a drum synthesizer in C++: `core/drum.c`, `core/dsp.c`, `core/drum_presets.c` and `core/drum_levels.c`
  are its engine, its 58 drums and its 12 kits, ported to C. `tools/thump_ref.cpp` and `make ref` build thump's own
  engine from its source (not in this repository) to check the port against.
- **BARE!** (https://github.com/willbearfruits/bare): the perform effects follow its performance page.
- **keys**: `desktop/midi_alsa.c` is adapted from its ALSA MIDI code.

## Not included

- **PortMaster** is not part of RHYTHM. When it is installed, `Rhythm.sh` reads its controller mapping, with the
  lines PortMaster's own ports use to find it; without it, RHYTHM reads the R36S's buttons itself.

## Named after

The factory drums and kits are named after the machines and styles they sound like (808 Classic, 909 Snare, Lo-Fi
Dusty and so on): Roland's TR-808 and TR-909 among them. They are synthesized; no sound of those machines is used. The
SYNC page's presets name Teenage Engineering's Pocket Operators and Korg's volca series, and the delay is modelled on
pedals built around the PT2399 chip. The R36S is the handheld it runs on and ArkOS the system on its card. The names
describe; none of the companies or people named is involved with RHYTHM or endorses it.
