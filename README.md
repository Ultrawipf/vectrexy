# Vectrexy

Vectrexy is a [Vectrex](https://en.wikipedia.org/wiki/Vectrex) emulator programmed in C++.

This project is open source and available on GitHub: https://github.com/amaiorano/vectrexy

## Download Latest Build

### Build status ![master branch status](https://github.com/amaiorano/vectrexy/actions/workflows/ci.yml/badge.svg?branch=master)

* [Vectrexy for Windows 64-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-windows-x64.zip)

* [Vectrexy for Windows 32-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-windows-x86.zip)

* [Vectrexy for Linux 64-bit](https://dl.cloudsmith.io/public/vectrexy/vectrexy/raw/files/vectrexy-ubuntu-x64.zip)

\
[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Package repository hosting is graciously provided by  [Cloudsmith](https://cloudsmith.com).
Cloudsmith is the only fully hosted, cloud-native, universal package management solution, that
enables your organization to create, store and share packages in any format, to any place, with total
confidence.

## Twitch Development

I regularly stream part of the development on Twitch [right here](https://www.twitch.tv/daroou2). Follow me to know when I'm streaming!

## Compatibility

See the [Vectrexy Compatibility List](docs/vectrexy-compatilibity-list.md) for the list of games that Vectrexy can run.

## Controls

| Devices     | Player 1  | Player 2  |
| ----------- | --------- | --------- |
| No gamepads | Keyboard  | N/A       |
| 1 gamepad   | Gamepad 1 | Keyboard  |
| 2 gamepads  | Gamepad 1 | Gamepad 2 |

Keyboard key bindings: ASDF + Arrow keys

## Overlays

The Vectrex display is black & white, so to add color, each game cartridge came with a transparent colored overlay that would be slotted in front of the screen. For emulation purposes, you should be able to find png files for these overlays. If you place these png file in the `data/overlays` folder, Vectrexy will attempt to match the rom's file name to the overlay name using "fuzzy" string matching (in other words, the file names do not need to match exactly).

## Laser Projector Output

A Vectrex draws with a moving beam, which is also how an ILDA laser projector
draws, so the emulator can stream its vector list to one instead of only
rasterising it to a window. Enable it under **Settings > Laser Output**: a
checkbox, a host and a port (default `127.0.0.1:12000`). One UDP packet is sent
per emulated frame, containing the same line list the GL window draws, in beam
order.

This sends geometry, not points. Galvo speeds, dwells, blanking and the point
budget are the receiver's job.

### Wire format

Little-endian, no padding between fields (the project targets x86/x64 only). A
packet is a 12-byte header followed by `lineCount` 20-byte line records:

| Offset | Type       | Field         | Notes                                              |
| ------ | ---------- | ------------- | -------------------------------------------------- |
| 0      | `uint32`   | `magic`       | `'V','L','S','R'` in that byte order (`0x52534C56`) |
| 4      | `uint16`   | `version`     | currently `1`                                       |
| 6      | `uint32`   | `frameNumber` | increments per packet                               |
| 10     | `uint16`   | `lineCount`   | number of records that follow                       |

Each record, repeated `lineCount` times:

| Offset | Type      | Field        | Notes            |
| ------ | --------- | ------------ | ---------------- |
| 0      | `float32` | `x0`         | start point x    |
| 4      | `float32` | `y0`         | start point y    |
| 8      | `float32` | `x1`         | end point x      |
| 12     | `float32` | `y1`         | end point y      |
| 16     | `float32` | `brightness` | `0..1`           |

Coordinates are in Vectrex screen space, roughly `-128..128` on both axes (see
`Screen.cpp`). Lines are in the order the emulated beam drew them, so segments
that share an endpoint are usually adjacent - worth exploiting, since a receiver
can then draw a whole connected run without blanking.

Because UDP has no delivery guarantee, `frameNumber` is there so a receiver can
notice dropped or reordered packets. A packet carries at most 3000 lines to stay
inside one datagram; anything beyond that in a frame is dropped.

There is one important subtlety. The emulator frames its work by CPU cycle
budget, not by the Vectrex's own redraw: at 60fps it runs 25,000 cycles while a
full Vectrex redraw takes about 30,000. Those beat at 5:6, so **a single packet
is generally a fragment of a picture rather than a whole one**, and consecutive
packets cut the picture at different points. A receiver that treats one packet
as one frame will see the image churn even when it is completely static. Accumulate
packets over a window and de-duplicate repeated segments instead.

### Simplified rendering

Two authentic Vectrex behaviours survive rasterisation fine but do not survive a
galvo, so **Settings > Simplified Rendering** can optionally remove them. All of
it is off by default, and none of it changes emulation - every CPU instruction
still executes and only the emitted line list differs.

- **Text** - the BIOS font at `$F9D4`-`$FBB4` is a *bitmap* font drawn by
  sweeping the beam and blanking it per raster row, which shreds on a laser.
  *Stroke text* suppresses those vectors and substitutes a single-stroke vector
  font, hooking the BIOS `Print_Str` entry at `$F495` (so it covers every BIOS
  print routine, but not cartridges that draw their own glyphs). *Both* draws the
  stroke font over the bitmap one, for calibration.
- **Merge dashed lines** - the startup and Mine Storm borders are genuinely
  drawn as dashes. This stitches near-collinear runs back into single lines,
  with *Merge max gap* setting how far apart pieces may be.
- **Close corners** - independent of merging. The emulated beam only draws once
  its ramp settles, so it stops slightly short at each corner and leaves a notch.

A fourth BIOS, **Laser (solid border)**, is selectable in the Bios menu. It is
`System.bin` with 8 bytes patched so the startup logo draws a single solid
border instead of two dashed ones, and stops pulsing (the pulse redraws the logo
twice per frame, which doubles the text). See `tools/patch_bios_border.py`,
which documents the patched addresses and can regenerate it.

## What's a Vectrex and why did you write this emulator?

The Vectrex is a really cool and unique video game console that was released in 1982. What made it unique was that it came with its own screen and displayed vector-based graphics, rather than the typical sprite/raster based graphics of most game consoles. My uncle had gotten me a Vectrex when I was only 8 years old, and I still have it, and it's still awesome.

## Credits

Although the emulator core is written by me, Antonio Maiorano (Daroou2), it makes use of third party libraries, and is packaged with overlays created by other people. I hope I've got everyone covered here; if not, please let me know and I'll be happy to correct this list.

- Overlays: THK-Hyperspin, Gigapig-Hyperspin, Nosh01-GitHub, and other unknown authors.
- FastBoot and SkipBoot bios roms: Franck Chevassu, author of the exellent Vectrex emulator, [ParaJVE](http://www.vectrex.fr/ParaJVE/)
- SDL2: [SDL2 Credits](https://www.libsdl.org/credits.php)
- GLEW: [GLEW authors](https://github.com/nigels-com/glew#authors)
- GLM: [G-Truc Creations](http://www.g-truc.net/)
- stb: [Sean Barrett (Nothings)](http://nothings.org/)
- Dear ImGui: [omar (ocornut)](http://www.miracleworld.net/)
- linenoise: [Salvatore Sanfilippo (antirez)](http://invece.org/)
- noc: [Guillaume Chereau](https://blog.noctua-software.com/)

## Building the code

### Windows

Install:
* [CMake](https://cmake.org/)
* [Visual Studio](https://www.visualstudio.com/downloads/)

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake ..
cmake --build .
```

### Ubuntu

Install:
* [CMake](https://cmake.org/)
* gcc 8 or higher

Install a compiler and some Linux-specific libs we depend on:
```bash
sudo apt-get install g++-8 libgtk2.0-dev
```

SDL2 has many dependencies, some of which you may need to install:
```
# SDL static lib dependencies (see https://hg.libsdl.org/SDL/file/default/docs/README-linux.md)
# Alternatively, you can just 'apt-get libsdl2-dev' to build against the dynamic library

sudo apt-get install build-essential mercurial make cmake autoconf automake libtool libasound2-dev libpulse-dev libaudio-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxinerama-dev libxxf86vm-dev libxss-dev libgl1-mesa-dev libesd0-dev libdbus-1-dev libudev-dev libgles1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev libibus-1.0-dev fcitx-libs-dev libsamplerate0-dev libsndio-dev
```

Clone and build vectrexy using CMake:
```bash
git clone --recursive https://github.com/amaiorano/vectrexy.git
cd vectrexy
mkdir build && cd build
cmake ..
cmake --build .
```

### Extra CMake Build Args

Use ```-D<VAR_NAME>=<VALUE>``` from the CMake CLI, or use cmake-gui to set these.

#### BUILD_SHARED_LIBS=on|off (Default: off)

If enabled, builds a DLL/.so version.

**NOTE**: On Windows, vcpkg's "static" triplets (e.g. x64-windows-static) create static libs that link against the static CRT (/MT), while CMake generates shared library builds that link against the dynamic CRT (/MD). Thus, when building, the linker will emit: `LINK : warning LNK4098: defaultlib 'LIBCMT' conflicts with use of other libs; use /NODEFAULTLIB:library`. You can ignore this for the most part; however, you can fix this warning by creating a custom vcpkg triplet, e.g. x64-windows-static-md.cmake, that is a copy of x64-windows-static.cmake, except with `set(VCPKG_CRT_LINKAGE dynamic)`. If you use this triplet to build dependencies with vcpkg, and specify it as CMake's `VCPKG_TARGET_TRIPLET`, all libraries will use the dynamic CRT, and no warning will be emitted by the linker.

#### DEBUG_UI=on|off (Default: on)

If enabled, the in-game debug UI can be displayed. Mostly useful for development.

#### ENGINE_TYPE=null|sdl (Default: sdl)

The type of engine to use. By default, SDL is used. If "null" is specified, the emulator will execute without any audio or visuals; however, the debugger will work, which can be useful for testing the emulator, or as a starting point for a new engine type.

## Contributing

As the emulator is still in early stages of development, I generally won't be looking at or accepting pull requests. Once the project has matured enough, this will likely change. If you wish, [follow my stream](https://www.twitch.tv/daroou2) and make suggestions in chat instead.
