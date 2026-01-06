# Gens-rr with Automation Extension

Fork of Gens-rr emulator with screenshot automation for ROM analysis.

## Build

**Tested on:** Visual Studio 2022, Platform Toolset v143

```cmd
cd Gens-rr
MSBuild.exe gens.vcxproj -p:Configuration=Release -p:Platform=Win32 -p:PlatformToolset=v143
```

Output: `Gens-rr/Output/Gens.exe`

## Automation Features

### New Command-line Arguments

| Argument | Description |
|----------|-------------|
| `-screenshot-interval N` | Capture screenshot every N frames (0 = disabled) |
| `-screenshot-dir path` | Directory to save screenshots |
| `-reference-dir path` | Directory with reference screenshots (enables compare mode) |
| `-max-frames N` | Stop emulation after N frames |
| `-max-diffs N` | Stop after N differences found (default: 10) |
| `-frameskip N` | Set frame skip (-1 to 8) |
| `-turbo` | Enable turbo mode |

### Record Mode

Saves screenshots at specified intervals. Filenames are frame numbers (e.g., `000060.png`).

```cmd
Gens.exe -rom game.bin -play movie.gmv -screenshot-interval 60 -screenshot-dir reference/ -max-frames 90000 -turbo -frameskip 8
```

### Compare Mode

Compares current frame with reference screenshot. Saves only differing frames.

```cmd
Gens.exe -rom modified.bin -play movie.gmv -screenshot-interval 60 -reference-dir reference/ -screenshot-dir diffs/ -max-diffs 10 -turbo -frameskip 8
```

## Use Case

Designed for analyzing disassembled ROMs:

1. Record reference screenshots during TAS playback
2. For each procedure: disable it (add RTS), rebuild ROM, compare screenshots
3. Identify which procedures affect visual output by frame number

## Changes

- `src/automation.cpp` / `src/automation.h` - Automation module
- `src/ParseCmdLine.cpp` - Command-line parsing
- `src/G_ddraw.cpp` - Screenshot capture hook
- `gens.vcxproj` - VS2022 compatibility (removed nafxcw.lib, added /SAFESEH:NO)
- `src/luascript.cpp` - Fixed `new char[]` for modern C++
- `src/OpenArchive.cpp` - Added missing `#include <string>`
