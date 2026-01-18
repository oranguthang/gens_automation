#ifndef AUTOMATION_H
#define AUTOMATION_H

// Automation module for screenshot capture and comparison
// Used for ROM analysis by comparing visual output

// Global variables (defined in automation.cpp)
extern int ScreenshotInterval;     // Capture every N frames (0 = disabled)
extern int MaxFrames;              // Stop after N frames (0 = no limit)
extern int MaxDiffs;               // Stop after N screenshot differences (default 10)
extern int DiffCount;              // Current screenshot difference count
extern int MaxMemoryDiffs;         // Stop after N memory differences (default 10)
extern int MemoryDiffCount;        // Current memory difference count
extern int SaveMemoryOnlyAfterVisual; // Only save memory diffs after first visual diff (0 = disabled, 1 = enabled)
extern char ScreenshotDir[1024];   // Directory to save screenshots
extern char ReferenceDir[1024];    // Reference screenshots dir (empty = record mode)
extern unsigned char DiffColor[4]; // BGRA color for diff highlighting (default: pink)
extern int CompareStateDumpsMode;  // Compare memory dumps instead of screenshots (0 = disabled)

// Trace automation parameters
extern unsigned int TraceBreakpointPC;    // PC address to trigger trace (0 = disabled)
extern int TraceFramesAfterBreak;         // Number of frames to trace after breakpoint
extern int TraceFrameCounter;             // Counter for frames since breakpoint
extern char TraceLogPath[1024];           // Path to trace log file
extern int TraceActive;                   // Trace is currently active (breakpoint hit)
extern int TraceBreakpointHit;            // Breakpoint was hit at least once
extern int TraceStartFrame;               // Start tracing at this frame (0 = disabled)
extern int TraceEndFrame;                 // Stop tracing at this frame (0 = disabled)
extern int TraceCompleted;                // Trace has finished (prevents restart)

// Initialize automation module (call from WinMain after config load)
void Automation_Init();

// Reset state for new run (call when starting movie playback)
void Automation_Reset();

// Called every frame during movie playback
// screen - pointer to MD_Screen buffer
// mode - (Bits32 ? 2 : 0) | (Mode_555 ? 1 : 0)
// Hmode - VDP horizontal mode (320 or 256)
// Vmode - VDP vertical mode (240 or 224)
void Automation_OnFrame(int frameCount, void* screen, int mode, int Hmode, int Vmode);

// Save screenshot to specific file
// Returns: 1 on success, 0 on failure
int Save_Shot_To_File(void* screen, int mode, int Hmode, int Vmode, const char* filename);

// Compare current screen with reference PNG
// Returns: true if screens match, false if different
bool Compare_With_Reference(void* screen, int mode, int Hmode, int Vmode, const char* refPath);

// Load PNG file into BGRA buffer
// Returns: true on success, fills buffer and sets width/height
bool Load_PNG(const char* path, unsigned char* buffer, int bufferSize, int* width, int* height);

// Trace automation functions
// Called before each instruction to check for breakpoint
void Trace_CheckBreakpoint(unsigned int pc);

// Called each frame to handle trace frame counting
void Trace_OnFrame(int frameCount);

// Initialize trace log file
void Trace_Init();

// Write instruction to trace log
void Trace_LogInstruction(unsigned int pc, const char* disasm, 
                          unsigned int* dregs, unsigned int* aregs, unsigned int sr);

// Write memory access to trace log
void Trace_LogMemAccess(const char* type, unsigned int pc, unsigned int addr, 
                        unsigned int value, int size);

// Close trace log
void Trace_Close();

#endif // AUTOMATION_H
