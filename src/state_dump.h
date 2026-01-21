#ifndef STATE_DUMP_H
#define STATE_DUMP_H

// State Dump module for capturing complete emulator state
// Used for memory-level ROM analysis and debugging

// Section IDs (must match Python genstate_format.py)
#define SECTION_M68K_RAM    0x01
#define SECTION_M68K_REGS   0x02
#define SECTION_VDP_VRAM    0x10
#define SECTION_VDP_CRAM    0x11
#define SECTION_VDP_VSRAM   0x12
#define SECTION_VDP_REGS    0x13
#define SECTION_Z80_RAM     0x20
#define SECTION_Z80_REGS    0x21
#define SECTION_YM2612      0x30  // FM sound chip (5328 bytes)
#define SECTION_PSG         0x31  // PSG sound generator (~64 bytes)
#define SECTION_SRAM        0x40  // Battery-backed SRAM (up to 64KB)

// Global variables (defined in state_dump.cpp)
extern int StateDumpInterval;      // Dump every N frames (0 = disabled)
extern int StateDumpStart;         // Start dumping from this frame (0 = start immediately)
extern int StateDumpEnd;           // Stop dumping after this frame (0 = no limit)
extern char StateDumpDir[1024];    // Directory to save .genstate files
extern int StateDumpWithScreenshots; // Save state dumps alongside screenshots (0 = disabled)

// Initialize state dump module
void StateDump_Init();

// Reset state for new run
void StateDump_Reset();

// Called every frame during emulation
// Checks if state should be dumped and performs the dump
void StateDump_OnFrame(int frameCount);

// Dump complete emulator state to .genstate file
// Returns: 1 on success, 0 on failure
int StateDump_DumpState(int frameNumber);

// Dump state with custom filename (without extension)
// Used when saving alongside screenshots to match their naming format
// Example: StateDump_DumpStateToFile("reference/tas", "000020")
// Creates: reference/tas/000020.genstate
int StateDump_DumpStateToFile(const char* directory, const char* basename);

// Check if state should be dumped at this frame
bool StateDump_ShouldDump(int frameCount);

#endif // STATE_DUMP_H
