#ifndef AUTOMATION_H
#define AUTOMATION_H

// Automation module for screenshot capture and comparison
// Used for ROM analysis by comparing visual output

// Global variables (defined in automation.cpp)
extern int ScreenshotInterval;     // Capture every N frames (0 = disabled)
extern int MaxFrames;              // Stop after N frames (0 = no limit)
extern int MaxDiffs;               // Stop after N differences (default 10)
extern int DiffCount;              // Current difference count
extern char ScreenshotDir[1024];   // Directory to save screenshots
extern char ReferenceDir[1024];    // Reference screenshots dir (empty = record mode)

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

#endif // AUTOMATION_H
