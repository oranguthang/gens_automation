// Binary trace implementation for Gens-rr emulator
// Compact binary format with memory access aggregation

#include <stdio.h>
#include <string.h>
#include "bintrace.h"

// Global state
int BinTraceActive = 0;
int BinTraceStartFrame = 0;
int BinTraceEndFrame = 0;
int BinTraceLogExec = 0;      // Off by default (saves space)
int BinTraceLogVDP = 1;       // On by default
int BinTraceLogDMA = 1;       // On by default
char BinTracePath[1024] = "";

// Internal state
static FILE* trace_file = NULL;
static uint32_t event_count = 0;
static uint32_t first_frame = 0;
static uint32_t last_frame = 0;
static uint32_t current_frame = 0;
static int header_written = 0;

// Aggregation buffer for sequential memory accesses
static struct BinTraceAggBuffer agg_buffer = {0};

// Forward declarations
static void write_event(const void* data, size_t size);
static void flush_single_event(void);
static void flush_block_event(void);

void BinTrace_Init(const char* path)
{
    if (trace_file)
        BinTrace_Close();

    trace_file = fopen(path, "wb");
    if (!trace_file)
        return;

    // Write placeholder header (will be updated on close)
    struct BinTraceHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, "BTRC", 4);
    header.version = 0x0001;
    fwrite(&header, sizeof(header), 1, trace_file);

    // Reset state
    event_count = 0;
    first_frame = 0;
    last_frame = 0;
    current_frame = 0;
    header_written = 1;
    memset(&agg_buffer, 0, sizeof(agg_buffer));

    BinTraceActive = 1;
}

void BinTrace_Close(void)
{
    if (!trace_file)
        return;

    // Flush any pending data
    BinTrace_Flush();

    // Seek back and update header
    struct BinTraceHeader header;
    memcpy(header.magic, "BTRC", 4);
    header.version = 0x0001;
    header.flags = 0;
    header.start_frame = first_frame;
    header.end_frame = last_frame;
    header.event_count = event_count;
    header.reserved[0] = 0;
    header.reserved[1] = 0;
    header.reserved[2] = 0;

    fseek(trace_file, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, trace_file);

    fclose(trace_file);
    trace_file = NULL;
    BinTraceActive = 0;
    header_written = 0;
    // Clear path to prevent re-init after close (for delayed start mode)
    BinTracePath[0] = '\0';
}

static void write_event(const void* data, size_t size)
{
    if (!trace_file)
        return;

    fwrite(data, size, 1, trace_file);
    event_count++;
}

void BinTrace_FrameMarker(uint32_t frame)
{
    if (!trace_file)
        return;

    // Flush aggregation buffer on frame boundary
    BinTrace_Flush();

    // Update frame tracking
    if (first_frame == 0 && event_count == 0)
        first_frame = frame;
    last_frame = frame;
    current_frame = frame;

    // Write frame event
    struct BinTraceFrameEvent evt;
    evt.header.type = EVT_FRAME;
    evt.header.flags = 0;
    evt.header.frame_delta = 0;
    evt.frame = frame;

    write_event(&evt, sizeof(evt));
}

void BinTrace_Flush(void)
{
    if (!agg_buffer.active)
        return;

    if (agg_buffer.len <= 4)
    {
        // Small access - write as single event
        flush_single_event();
    }
    else
    {
        // Large access - write as block event
        flush_block_event();
    }

    // Reset buffer
    agg_buffer.active = 0;
    agg_buffer.len = 0;
}

static void flush_single_event(void)
{
    struct BinTraceMemEvent evt;
    evt.header.type = agg_buffer.type;
    evt.header.flags = 0;
    evt.header.frame_delta = 0;
    evt.pc = agg_buffer.pc;
    evt.addr = agg_buffer.start_addr;
    evt.size = agg_buffer.len;

    // Reconstruct value from buffer (big-endian, as M68K is big-endian)
    evt.value = 0;
    for (int i = 0; i < agg_buffer.len; i++)
    {
        evt.value = (evt.value << 8) | agg_buffer.data[i];
    }

    // Check for ROM access
    if (agg_buffer.start_addr < 0x400000)
        evt.header.flags |= FLAG_ROM_ACCESS;
    else if (agg_buffer.start_addr >= 0xFF0000)
        evt.header.flags |= FLAG_RAM_ACCESS;

    // Check if value looks like a pointer (for 4-byte reads)
    if (agg_buffer.len == 4 && BinTrace_IsPointer(evt.value))
        evt.header.flags |= FLAG_POINTER;

    write_event(&evt, sizeof(evt));
}

static void flush_block_event(void)
{
    // Write header
    struct BinTraceBlockEvent evt;
    evt.header.type = (agg_buffer.type == EVT_READ) ? EVT_READ_BLOCK : EVT_WRITE_BLOCK;
    evt.header.flags = 0;
    evt.header.frame_delta = 0;
    evt.pc = agg_buffer.pc;
    evt.addr = agg_buffer.start_addr;
    evt.data_len = agg_buffer.len;
    evt.reserved = 0;

    // Check for ROM access
    if (agg_buffer.start_addr < 0x400000)
        evt.header.flags |= FLAG_ROM_ACCESS;
    else if (agg_buffer.start_addr >= 0xFF0000)
        evt.header.flags |= FLAG_RAM_ACCESS;

    write_event(&evt, sizeof(evt));

    // Write data (not counted as separate event)
    if (trace_file)
    {
        fwrite(agg_buffer.data, agg_buffer.len, 1, trace_file);

        // Pad to 4-byte boundary for alignment
        int pad = (4 - (agg_buffer.len & 3)) & 3;
        if (pad > 0)
        {
            uint8_t zeros[4] = {0};
            fwrite(zeros, pad, 1, trace_file);
        }
    }
}

void BinTrace_MemAccess(uint8_t type, uint32_t pc, uint32_t addr, uint32_t value, int size)
{
    if (!trace_file || !BinTraceActive)
        return;

    // Normalize type to EVT_READ or EVT_WRITE
    uint8_t base_type = (type == EVT_READ || type == EVT_READ_BLOCK) ? EVT_READ : EVT_WRITE;

    // Check if we can aggregate this access
    if (agg_buffer.active &&
        agg_buffer.type == base_type &&
        addr == agg_buffer.next_addr &&
        agg_buffer.len + size <= BINTRACE_BUFFER_SIZE)
    {
        // Append to existing buffer
        // Store in big-endian order (M68K native)
        if (size == 1)
        {
            agg_buffer.data[agg_buffer.len] = value & 0xFF;
        }
        else if (size == 2)
        {
            agg_buffer.data[agg_buffer.len] = (value >> 8) & 0xFF;
            agg_buffer.data[agg_buffer.len + 1] = value & 0xFF;
        }
        else if (size == 4)
        {
            agg_buffer.data[agg_buffer.len] = (value >> 24) & 0xFF;
            agg_buffer.data[agg_buffer.len + 1] = (value >> 16) & 0xFF;
            agg_buffer.data[agg_buffer.len + 2] = (value >> 8) & 0xFF;
            agg_buffer.data[agg_buffer.len + 3] = value & 0xFF;
        }
        agg_buffer.len += size;
        agg_buffer.next_addr = addr + size;
    }
    else
    {
        // Cannot aggregate - flush existing buffer and start new one
        BinTrace_Flush();

        agg_buffer.active = 1;
        agg_buffer.type = base_type;
        agg_buffer.pc = pc;
        agg_buffer.start_addr = addr;
        agg_buffer.next_addr = addr + size;
        agg_buffer.len = size;

        // Store in big-endian order
        if (size == 1)
        {
            agg_buffer.data[0] = value & 0xFF;
        }
        else if (size == 2)
        {
            agg_buffer.data[0] = (value >> 8) & 0xFF;
            agg_buffer.data[1] = value & 0xFF;
        }
        else if (size == 4)
        {
            agg_buffer.data[0] = (value >> 24) & 0xFF;
            agg_buffer.data[1] = (value >> 16) & 0xFF;
            agg_buffer.data[2] = (value >> 8) & 0xFF;
            agg_buffer.data[3] = value & 0xFF;
        }
    }
}

void BinTrace_VRAMAccess(uint8_t vdp_type, uint32_t pc, uint32_t addr, uint32_t value, int size)
{
    if (!trace_file || !BinTraceActive || !BinTraceLogVDP)
        return;

    // Flush aggregation buffer before VDP access
    BinTrace_Flush();

    struct BinTraceVDPEvent evt;

    // Map VDP access type to event type
    // Read types: 5=VRAM, 6=CRAM, 7=VSRAM
    // Write types: 9=VRAM, 10=CRAM, 11=VSRAM
    switch (vdp_type)
    {
        case 5:  evt.header.type = EVT_VRAM_READ; break;
        case 6:  evt.header.type = EVT_CRAM_READ; break;
        case 7:  evt.header.type = EVT_VSRAM_READ; break;
        case 9:  evt.header.type = EVT_VRAM_WRITE; break;
        case 10: evt.header.type = EVT_CRAM_WRITE; break;
        case 11: evt.header.type = EVT_VSRAM_WRITE; break;
        default: return; // Unknown type
    }

    evt.header.flags = 0;
    evt.header.frame_delta = 0;
    evt.pc = pc;
    evt.addr = addr & 0xFFFF;
    evt.size = size;
    evt.reserved = 0;
    evt.value = value;

    write_event(&evt, sizeof(evt));
}

void BinTrace_DMA(uint32_t pc, uint32_t src, uint32_t dst, uint16_t len, uint8_t dst_type)
{
    if (!trace_file || !BinTraceActive || !BinTraceLogDMA)
        return;

    // Flush aggregation buffer before DMA
    BinTrace_Flush();

    struct BinTraceDMAEvent evt;
    evt.header.type = EVT_DMA;
    evt.header.flags = 0;
    evt.header.frame_delta = 0;
    evt.pc = pc;
    evt.src = src;
    evt.dst = dst;
    evt.len = len;
    evt.dst_type = dst_type;
    evt.reserved[0] = 0;
    evt.reserved[1] = 0;
    evt.reserved[2] = 0;

    // Mark ROM source
    if (src < 0x400000)
        evt.header.flags |= FLAG_ROM_ACCESS;
    else if (src >= 0xFF0000)
        evt.header.flags |= FLAG_RAM_ACCESS;

    write_event(&evt, sizeof(evt));
}

int BinTrace_IsPointer(uint32_t value)
{
    // Heuristic: value looks like a valid M68K address
    // Valid ranges for Genesis:
    // - ROM: 0x000000 - 0x3FFFFF (up to 4MB)
    // - RAM: 0xFF0000 - 0xFFFFFF (64KB, mirrored)
    // - Also accept 0xE00000 - 0xFFFFFF (RAM mirrors)

    // Even addresses only (M68K alignment requirement for word/long)
    if (value & 1)
        return 0;

    // Check valid ranges
    if (value < 0x400000)  // ROM
        return 1;
    if (value >= 0xE00000 && value <= 0xFFFFFF)  // RAM / mirrors
        return 1;

    return 0;
}
