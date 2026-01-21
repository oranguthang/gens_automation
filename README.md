# Gens-rerecording with Automation Extension

Fork of Gens-rerecording emulator with screenshot automation, memory state dumps, and binary tracing for ROM analysis.

## Build

**Requirements:** Visual Studio 2022, Platform Toolset v143

```cmd
make
```

Output: `Output/Gens.exe`

## Features

### Screenshot Automation

Capture and compare screenshots during TAS playback for ROM analysis.

| Argument | Description |
|----------|-------------|
| `-screenshot-interval N` | Capture screenshot every N frames (0 = disabled) |
| `-screenshot-dir path` | Directory to save screenshots |
| `-reference-dir path` | Directory with reference screenshots (enables compare mode) |
| `-max-frames N` | Stop emulation after N frames |
| `-max-diffs N` | Stop after N visual differences found (default: 10) |
| `-diff-color COLOR` | Overlay color for diff images: pink, red, green, blue, yellow, cyan, white, orange |

**Record mode** - save reference screenshots:
```cmd
Gens.exe -rom game.bin -play movie.gmv -screenshot-interval 60 -screenshot-dir reference/ -max-frames 90000 -turbo -frameskip 8
```

**Compare mode** - find visual differences:
```cmd
Gens.exe -rom modified.bin -play movie.gmv -screenshot-interval 60 -reference-dir reference/ -screenshot-dir diffs/ -max-diffs 10 -turbo -frameskip 8
```

### Memory State Dumps

Capture complete emulator state for detailed debugging.

| Argument | Description |
|----------|-------------|
| `-save-state-dumps` | Save .genstate files alongside screenshots |
| `-compare-state-dumps` | Compare RAM states, exit on memory diff |
| `-max-memory-diffs N` | Stop after N memory differences found |
| `-memory-after-visual 1` | Only save memory diffs after first visual diff |
| `-no-memory-diffs` | Disable memory diff CSV files |

State dump sections (~130KB per frame):
- M68K_RAM (64KB), M68K_REGS (72B)
- VDP_VRAM (64KB), VDP_CRAM (128B), VDP_VSRAM (80B), VDP_REGS (24B)
- Z80_RAM (8KB)
- YM2612 (5328B) - FM sound chip
- PSG (~64B) - sound generator
- SRAM (64KB) - battery-backed RAM

### Binary Tracing

Compact binary format for memory access and DMA logging (~20 bytes/event vs ~250 for text).

| Argument | Description |
|----------|-------------|
| `-bintrace path` | Output file for binary trace |
| `-bintrace-start N` | Start tracing at frame N |
| `-bintrace-end N` | Stop tracing at frame N |
| `-bintrace-vdp 1` | Include VRAM/CRAM/VSRAM access |
| `-bintrace-dma 1` | Include DMA transfers |

Event types: READ, WRITE, BLOCK (aggregated sequential access), VRAM, CRAM, VSRAM, DMA.

### CPU Tracing

Frame-based CPU instruction tracing.

| Argument | Description |
|----------|-------------|
| `-trace-start N` | Start CPU trace at frame N |
| `-trace-end N` | Stop CPU trace at frame N (max 100 frames) |

### Other Options

| Argument | Description |
|----------|-------------|
| `-turbo` | Enable turbo mode |
| `-frameskip N` | Set frame skip (-1 to 8) |
| `-nosound` | Disable sound |
| `-window-x N` | Window X position |
| `-window-y N` | Window Y position |

## Use Case

Designed for analyzing disassembled ROMs with LLM assistance:

1. Record reference screenshots during TAS playback
2. For each procedure: disable it (add RTS), rebuild ROM, compare
3. Identify which procedures affect visual output by frame number
4. Use memory state dumps for detailed post-mortem analysis

## Changelog

### Automation System
- Screenshot capture/compare during TAS playback
- Diff visualization with colored overlays
- Auto-close on movie end or frame/diff limits
- Window positioning for multi-instance runs

### Memory Analysis
- Complete emulator state dumps (.genstate format)
- Memory-level comparison mode
- Dual comparison: visual + memory with independent counters
- `-memory-after-visual` to reduce disk usage

### Tracing
- Binary trace system for compact memory/DMA logging
- Frame-based CPU tracing mode
- Fixed delayed start tracing bug

### Build System
- Makefile for VS2022 (replaces manual MSBuild)
- Build dependencies included in repository
- Removed obsolete VS2008/2010 project files
