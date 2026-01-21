// Binary trace system for Gens-rr emulator
// Compact binary format for memory/VRAM/DMA tracing
// Designed for post-processing with Python (Graphviz visualization)

#ifndef BINTRACE_H
#define BINTRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Event types
enum BinTraceEventType {
    EVT_FRAME        = 0x00,  // Frame marker
    EVT_EXEC         = 0x01,  // Instruction execution (optional)
    EVT_READ         = 0x02,  // Single memory read
    EVT_WRITE        = 0x03,  // Single memory write
    EVT_READ_BLOCK   = 0x04,  // Block memory read (aggregated)
    EVT_WRITE_BLOCK  = 0x05,  // Block memory write (aggregated)
    EVT_VRAM_WRITE   = 0x10,  // VRAM write
    EVT_VRAM_READ    = 0x11,  // VRAM read
    EVT_CRAM_WRITE   = 0x12,  // CRAM write
    EVT_CRAM_READ    = 0x13,  // CRAM read
    EVT_VSRAM_WRITE  = 0x14,  // VSRAM write
    EVT_VSRAM_READ   = 0x15,  // VSRAM read
    EVT_DMA          = 0x20,  // DMA transfer
    EVT_POINTER_LOAD = 0x30,  // Potential pointer table load
};

// DMA destination types
enum BinTraceDMADest {
    DMA_DEST_VRAM  = 0,
    DMA_DEST_CRAM  = 1,
    DMA_DEST_VSRAM = 2,
};

// Flags for events
enum BinTraceFlags {
    FLAG_NONE       = 0x00,
    FLAG_ROM_ACCESS = 0x01,  // Access was from ROM region
    FLAG_RAM_ACCESS = 0x02,  // Access was from RAM region
    FLAG_POINTER    = 0x04,  // Value looks like a valid pointer
};

// File header (32 bytes, written at end when closing)
#pragma pack(push, 1)
struct BinTraceHeader {
    char     magic[4];      // "BTRC"
    uint16_t version;       // 0x0001
    uint16_t flags;         // Reserved
    uint32_t start_frame;   // First frame number
    uint32_t end_frame;     // Last frame number
    uint32_t event_count;   // Total number of events
    uint32_t reserved[3];   // Padding to 32 bytes
};

// Event header (4 bytes, common to all events)
struct BinTraceEventHeader {
    uint8_t  type;          // BinTraceEventType
    uint8_t  flags;         // Event-specific flags
    uint16_t frame_delta;   // Frames since last frame marker (0 if same frame)
};

// Frame marker event (8 bytes total)
struct BinTraceFrameEvent {
    struct BinTraceEventHeader header;
    uint32_t frame;         // Absolute frame number
};

// Single memory access event (12 bytes total)
struct BinTraceMemEvent {
    struct BinTraceEventHeader header;
    uint32_t pc;            // Program counter
    uint32_t addr : 24;     // Memory address (24-bit for M68K)
    uint32_t size : 8;      // Access size (1, 2, or 4 bytes)
    uint32_t value;         // Value read/written
};

// Block memory access event (variable size: 12 + data_len bytes)
struct BinTraceBlockEvent {
    struct BinTraceEventHeader header;
    uint32_t pc;            // Program counter (first access)
    uint32_t addr;          // Starting address
    uint16_t data_len;      // Number of bytes in block
    uint16_t reserved;      // Alignment padding
    // uint8_t data[];      // Variable-length data follows
};

// VDP memory access event (12 bytes total)
struct BinTraceVDPEvent {
    struct BinTraceEventHeader header;
    uint32_t pc;            // Program counter
    uint16_t addr;          // VDP address (VRAM/CRAM/VSRAM)
    uint8_t  size;          // Access size (1 or 2 bytes)
    uint8_t  reserved;      // Alignment
    uint32_t value;         // Value read/written
};

// DMA event (16 bytes total)
struct BinTraceDMAEvent {
    struct BinTraceEventHeader header;
    uint32_t pc;            // Program counter that triggered DMA
    uint32_t src;           // Source address
    uint16_t dst;           // Destination address (VRAM/CRAM/VSRAM)
    uint16_t len;           // Transfer length in bytes
    uint8_t  dst_type;      // Destination type (0=VRAM, 1=CRAM, 2=VSRAM)
    uint8_t  reserved[3];   // Alignment
};

// Pointer load event (16 bytes total)
struct BinTracePointerEvent {
    struct BinTraceEventHeader header;
    uint32_t pc;            // Program counter
    uint32_t table_addr;    // Address of pointer table entry
    uint32_t target_addr;   // Address the pointer points to
};
#pragma pack(pop)

// Aggregation buffer for sequential memory accesses
#define BINTRACE_BUFFER_SIZE 256

struct BinTraceAggBuffer {
    int      active;        // Buffer has data
    uint8_t  type;          // EVT_READ or EVT_WRITE
    uint32_t pc;            // PC of first access
    uint32_t start_addr;    // Starting address
    uint32_t next_addr;     // Next expected address
    int      len;           // Current length
    uint8_t  data[BINTRACE_BUFFER_SIZE];  // Accumulated data
};

// Global state variables
extern int BinTraceActive;          // Tracing is currently active
extern int BinTraceStartFrame;      // Frame to start tracing (0 = from beginning)
extern int BinTraceEndFrame;        // Frame to stop tracing (0 = no limit)
extern int BinTraceLogExec;         // Log instruction executions
extern int BinTraceLogVDP;          // Log VDP memory accesses
extern int BinTraceLogDMA;          // Log DMA transfers
extern char BinTracePath[1024];     // Output file path

// API functions
void BinTrace_Init(const char* path);
void BinTrace_Close(void);

// Memory access logging (called from tracer.cpp hooks)
void BinTrace_MemAccess(uint8_t type, uint32_t pc, uint32_t addr, uint32_t value, int size);

// VDP memory access logging
void BinTrace_VRAMAccess(uint8_t vdp_type, uint32_t pc, uint32_t addr, uint32_t value, int size);

// DMA transfer logging
void BinTrace_DMA(uint32_t pc, uint32_t src, uint32_t dst, uint16_t len, uint8_t dst_type);

// Frame marker (called each frame)
void BinTrace_FrameMarker(uint32_t frame);

// Flush aggregation buffer
void BinTrace_Flush(void);

// Check if address looks like a valid pointer (heuristic)
int BinTrace_IsPointer(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // BINTRACE_H
