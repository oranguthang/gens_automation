# Building with Docker

Visual Studio is one way to build Gens, not the only one. This path needs
Docker and nothing else, and works from a Windows, Linux or macOS host.

```bash
make -f Makefile.docker win-i386
```

produces `Output/Gens.exe`. The first run builds the toolchain image and
cross-compiles zlib and libpng, which takes a few minutes; after that the
layers are cached and a rebuild is seconds.

`make -f Makefile.docker shell` drops into the build image with the source
mounted, for iterating on the port without rebuilding the image.

## What the image contains

Three stages:

| Stage | Contents |
| --- | --- |
| `toolchain` | MinGW-w64 i686 cross-compiler, NASM, and a host compiler |
| `deps` | zlib and libpng cross-built into `/opt/cross` |
| `builder` | the sources, compiled and linked |
| `artifact` | nothing but `Gens.exe`, so `--output` can copy it to the host |

The host compiler is there for the two Starscream 68000 core generators:
`Star.c` is ordinary ANSI C that *emits* assembly, so it is compiled and run on
the build machine, and its output is then assembled for the target.

## Why the only target is 32-bit Windows

Not an oversight, and not something a build system can fix.

Gens' CPU cores are about 32,000 lines of 32-bit x86 assembly with no C
fallback -- `Cpu_68k.c` and `Cpu_Z80.c` only configure them. The memory
dispatch tables make the assumption explicit:

```c
{0xFF0000, 0xFFFFFF, (unsigned)&Ram_68k[0] - 0xFF0000},
```

A pointer cast to `unsigned`. There is no 64-bit build to be had without
rewriting the emulation cores, which is a different project and would put the
accuracy those cores are valued for at risk.

That decides macOS: 10.15 removed 32-bit execution entirely, so **a native
macOS binary is impossible**. A Mac can build the Windows target with this kit,
and will be able to run the emulator once a headless Linux target exists, in a
`linux/386` container. Neither is a native `.app`.

Linux is possible and not yet done. It needs the DirectDraw, DirectSound and
DirectInput layer plus the Win32 window replaced -- about 13,500 lines. For the
automation workflow that means replacing them with *nothing*: screenshots are
taken from `MD_Screen`, the VDP's own framebuffer, and state dumps and traces
read memory directly, so none of the automation path goes through the
presentation layer.

## What had to change in the sources

The port is small and every change is either a MSVC extension GCC does not
have, or a latent bug the MSVC toolchain hid.

**MSVC extensions.** Five `__asm` blocks rewritten in C (CD audio resampling
and mixing, and a byte-swapped word read); functional casts with multi-word
type names (`unsigned int (x)` becoming `(unsigned int)(x)`); `sizeof` applied
to a type without parentheses; token pastes that paste onto nothing
(`hook_##name##()`, `Keys_Def[y].##z`); a non-const reference bound to a
temporary; structured exception handling around
`InitializeCriticalSection`, replaced with the `AndSpinCount` variant that
reports failure by return value.

**Missing on GCC.** `min` and `max`, which MSVC's `<windows.h>` defines in C++
as well as C and Gens uses about 500 times unqualified. `src/mingw_compat.h`
provides them, force-included, after pulling in every standard header Gens uses
so nothing in libstdc++ is parsed with the macros already in force.
`DIK_PREVTRACK`, which MinGW's `dinput.h` calls `DIK_CIRCUMFLEX`.
`__forceinline`. A `StdAfx.h` for `7z/CPP/Common`, which never existed because
MSVC does not open the file when it is using a precompiled header.

**Case sensitivity.** 116 include lines named a header in the wrong case --
`cd_sys.h` for `Cd_sys.h`, `misc.h` for `Misc.h`. Windows and a Docker bind
mount do not care; the container's own filesystem does.

**Latent bugs the MSVC linker hid.** `tracer.cpp` defined its own `Current_PC`
and `Debug`, which `Debug.cpp` and `Gens.cpp` also define. The MSVC linker
keeps the first definition and warns, and both of those files link earlier, so
the tracer's copies were already being discarded and its `Debug = 1`
initializer never took effect. They are now `extern`, which keeps that
behaviour and stops it depending on link order. `lapi.h` had a full definition
of `lua_gettop` moved out of `lapi.c` to let it inline, which emits a copy in
every translation unit that includes it; MSVC's `__inline` merged them.

**Resources.** `windres` cannot parse `DLGINIT`, which the Visual Studio
resource editor emits for a combo box with design-time data. Gens is a plain
Win32 application with no MFC and nothing reads resource type 240, so the
MinGW build omits it. Resource file paths use forward slashes, which `rc.exe`
accepts too.

## Differences from the Visual Studio build

Both compile the same sources, with two deliberate exceptions.

`src/pngstuff.cpp` is excluded, which changes nothing: it is a seven-line
remnant that is not in `Gens.vcxproj` either.

libpng differs. The Visual Studio build pairs the vendored `src/png.h` (libpng
1.2.0) with `dependencies/libs/libpng.lib`. The MinGW build links the
toolchain's libpng 1.6 and therefore has to use that library's own headers --
1.2 headers with a 1.6 runtime is a struct-layout hazard, and it showed up
immediately as a runtime warning. Gens only uses the stable public API, so the
two files that touch libpng select the header by compiler.

## Verifying a build

Play a movie headlessly and read the result back:

```bash
Output/Gens.exe -rom game.bin -play movie.gmv \
    -screenshot-interval 200 -screenshot-dir shots \
    -max-frames 1000 -save-state-dumps -turbo -frameskip 0 -nosound
```

`-frameskip` multiplies the screenshot interval, because the interval counts
rendered frames while `-max-frames` counts emulated ones. With `-frameskip 8`
an interval of 100 produces a frame every 900.

A `.genstate` carries the full 64 KiB of work RAM in section `0x01`, so a known
field of the game under test is the strongest check that emulation is right and
not merely that the binary starts.
