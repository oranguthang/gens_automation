// State Dump module - captures complete emulator state for debugging
// Format: .genstate files matching Python genstate_format.py specification

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "state_dump.h"
#include "Mem_M68k.h"
#include "Cpu_68k.h"
#include "vdp_io.h"
#include "Mem_Z80.h"
#include "z80.h"
#include "ym2612.h"
#include "psg.h"

// Global variables
int StateDumpInterval = 0;
int StateDumpStart = 0;
int StateDumpEnd = 0;
char StateDumpDir[1024] = ".";
int StateDumpWithScreenshots = 0;

// Section table entry structure
struct SectionEntry {
    unsigned int section_id;
    unsigned int offset;
    unsigned int size;
    unsigned int flags;
};

void StateDump_Init()
{
    StateDumpInterval = 0;
    StateDumpStart = 0;
    StateDumpEnd = 0;
    strcpy(StateDumpDir, ".");
}

void StateDump_Reset()
{
    // Nothing to reset currently
}

bool StateDump_ShouldDump(int frameCount)
{
    // Check if dumping is enabled
    if (StateDumpInterval == 0)
        return false;

    // Check frame range
    if (StateDumpStart > 0 && frameCount < StateDumpStart)
        return false;

    if (StateDumpEnd > 0 && frameCount > StateDumpEnd)
        return false;

    // Check interval
    if (frameCount % StateDumpInterval != 0)
        return false;

    return true;
}

void StateDump_OnFrame(int frameCount)
{
    if (StateDump_ShouldDump(frameCount))
    {
        StateDump_DumpState(frameCount);
    }
}

// Calculate simple checksum of ROM (first 4 bytes for quick ID)
static unsigned int Calculate_ROM_Checksum()
{
    if (Rom_Size < 4)
        return 0;

    // Simple checksum: just use first few bytes as ID
    // This is enough to distinguish different ROMs
    unsigned int checksum = 0;
    for (int i = 0; i < 256 && i < (int)Rom_Size; i++)
    {
        checksum = (checksum << 1) ^ Rom_Data[i];
    }
    return checksum;
}

// Write little-endian 32-bit integer
static void Write_LE_U32(FILE* f, unsigned int value)
{
    unsigned char bytes[4];
    bytes[0] = (value >> 0) & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    fwrite(bytes, 1, 4, f);
}

// Write little-endian 64-bit integer
static void Write_LE_U64(FILE* f, unsigned long long value)
{
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++)
    {
        bytes[i] = (value >> (i * 8)) & 0xFF;
    }
    fwrite(bytes, 1, 8, f);
}

// Write file header
static void Write_Header(FILE* f, int frameNumber)
{
    // Magic: "GENSTATE" (8 bytes)
    fwrite("GENSTATE", 1, 8, f);

    // Version: 1 (4 bytes, LE)
    Write_LE_U32(f, 1);

    // Frame number (4 bytes, LE)
    Write_LE_U32(f, frameNumber);

    // Timestamp (8 bytes, LE)
    unsigned long long timestamp = (unsigned long long)time(NULL);
    Write_LE_U64(f, timestamp);

    // ROM checksum (4 bytes, LE)
    unsigned int checksum = Calculate_ROM_Checksum();
    Write_LE_U32(f, checksum);

    // Reserved (36 bytes) - fill with zeros
    unsigned char reserved[36] = {0};
    fwrite(reserved, 1, 36, f);
}

// Write section table entry
static void Write_Section_Entry(FILE* f, const SectionEntry& entry)
{
    Write_LE_U32(f, entry.section_id);
    Write_LE_U32(f, entry.offset);
    Write_LE_U32(f, entry.size);
    Write_LE_U32(f, entry.flags);
}

// Write end marker for section table
static void Write_End_Marker(FILE* f)
{
    Write_LE_U32(f, 0);
    Write_LE_U32(f, 0);
    Write_LE_U32(f, 0);
    Write_LE_U32(f, 0);
}

// Collect 68000 registers into buffer
static void Collect_M68K_Registers(unsigned char* buffer)
{
    unsigned char* ptr = buffer;

    // D0-D7 (8 × 4 bytes, LE)
    for (int i = 0; i < 8; i++)
    {
        unsigned int val = Context_68K.dreg[i];
        ptr[0] = (val >> 0) & 0xFF;
        ptr[1] = (val >> 8) & 0xFF;
        ptr[2] = (val >> 16) & 0xFF;
        ptr[3] = (val >> 24) & 0xFF;
        ptr += 4;
    }

    // A0-A7 (8 × 4 bytes, LE)
    for (int i = 0; i < 8; i++)
    {
        unsigned int val = Context_68K.areg[i];
        ptr[0] = (val >> 0) & 0xFF;
        ptr[1] = (val >> 8) & 0xFF;
        ptr[2] = (val >> 16) & 0xFF;
        ptr[3] = (val >> 24) & 0xFF;
        ptr += 4;
    }

    // PC (4 bytes, LE)
    unsigned int pc = Context_68K.pc;
    ptr[0] = (pc >> 0) & 0xFF;
    ptr[1] = (pc >> 8) & 0xFF;
    ptr[2] = (pc >> 16) & 0xFF;
    ptr[3] = (pc >> 24) & 0xFF;
    ptr += 4;

    // SR (4 bytes, LE) - status register is 16-bit but we write 32 for alignment
    unsigned int sr = Context_68K.sr;
    ptr[0] = (sr >> 0) & 0xFF;
    ptr[1] = (sr >> 8) & 0xFF;
    ptr[2] = 0;
    ptr[3] = 0;
}

// Collect VDP registers into buffer (24 bytes)
static void Collect_VDP_Registers(unsigned char* buffer)
{
    // VDP has 24 registers (0-23), each 1 byte
    // We extract them from VDP_Reg structure

    buffer[0] = VDP_Reg.Set1 & 0xFF;
    buffer[1] = VDP_Reg.Set2 & 0xFF;
    buffer[2] = VDP_Reg.Pat_ScrA_Adr & 0xFF;
    buffer[3] = VDP_Reg.Pat_Win_Adr & 0xFF;
    buffer[4] = VDP_Reg.Pat_ScrB_Adr & 0xFF;
    buffer[5] = VDP_Reg.Spr_Att_Adr & 0xFF;
    buffer[6] = VDP_Reg.Reg6 & 0xFF;
    buffer[7] = VDP_Reg.BG_Color & 0xFF;
    buffer[8] = VDP_Reg.Reg8 & 0xFF;
    buffer[9] = VDP_Reg.Reg9 & 0xFF;
    buffer[10] = VDP_Reg.H_Int & 0xFF;
    buffer[11] = VDP_Reg.Set3 & 0xFF;
    buffer[12] = VDP_Reg.Set4 & 0xFF;
    buffer[13] = VDP_Reg.H_Scr_Adr & 0xFF;
    buffer[14] = VDP_Reg.Reg14 & 0xFF;
    buffer[15] = VDP_Reg.Auto_Inc & 0xFF;
    buffer[16] = VDP_Reg.Scr_Size & 0xFF;
    buffer[17] = VDP_Reg.Win_H_Pos & 0xFF;
    buffer[18] = VDP_Reg.Win_V_Pos & 0xFF;
    buffer[19] = VDP_Reg.DMA_Length_L & 0xFF;
    buffer[20] = VDP_Reg.DMA_Length_H & 0xFF;
    buffer[21] = VDP_Reg.DMA_Src_Adr_L & 0xFF;
    buffer[22] = VDP_Reg.DMA_Src_Adr_M & 0xFF;
    buffer[23] = VDP_Reg.DMA_Src_Adr_H & 0xFF;
}

int StateDump_DumpState(int frameNumber)
{
    char filename[1280];
    sprintf(filename, "%s/%d.genstate", StateDumpDir, frameNumber);

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        // Failed to open file - maybe directory doesn't exist
        return 0;
    }

    // Prepare sections
    const int NUM_SECTIONS = 10;
    SectionEntry sections[NUM_SECTIONS];

    // Calculate section offsets
    int header_size = 64;
    int section_table_size = (NUM_SECTIONS + 1) * 16; // +1 for end marker
    int current_offset = header_size + section_table_size;

    // Section 0: 68000 RAM (64KB)
    sections[0].section_id = SECTION_M68K_RAM;
    sections[0].offset = current_offset;
    sections[0].size = 64 * 1024;
    sections[0].flags = 0;
    current_offset += sections[0].size;

    // Section 1: 68000 Registers (72 bytes)
    sections[1].section_id = SECTION_M68K_REGS;
    sections[1].offset = current_offset;
    sections[1].size = 72;
    sections[1].flags = 0;
    current_offset += sections[1].size;

    // Section 2: VDP VRAM (64KB)
    sections[2].section_id = SECTION_VDP_VRAM;
    sections[2].offset = current_offset;
    sections[2].size = 64 * 1024;
    sections[2].flags = 0;
    current_offset += sections[2].size;

    // Section 3: VDP CRAM (128 bytes - 64 colors × 2 bytes)
    sections[3].section_id = SECTION_VDP_CRAM;
    sections[3].offset = current_offset;
    sections[3].size = 128;
    sections[3].flags = 0;
    current_offset += sections[3].size;

    // Section 4: VDP VSRAM (80 bytes - 40 entries × 2 bytes)
    sections[4].section_id = SECTION_VDP_VSRAM;
    sections[4].offset = current_offset;
    sections[4].size = 80;
    sections[4].flags = 0;
    current_offset += sections[4].size;

    // Section 5: VDP Registers (24 bytes)
    sections[5].section_id = SECTION_VDP_REGS;
    sections[5].offset = current_offset;
    sections[5].size = 24;
    sections[5].flags = 0;
    current_offset += sections[5].size;

    // Section 6: Z80 RAM (8KB)
    sections[6].section_id = SECTION_Z80_RAM;
    sections[6].offset = current_offset;
    sections[6].size = 8 * 1024;
    sections[6].flags = 0;
    current_offset += sections[6].size;

    // Section 7: YM2612 FM sound chip (5328 bytes)
    sections[7].section_id = SECTION_YM2612;
    sections[7].offset = current_offset;
    sections[7].size = 0x14d0;  // 5328 bytes
    sections[7].flags = 0;
    current_offset += sections[7].size;

    // Section 8: PSG sound generator (64 bytes)
    sections[8].section_id = SECTION_PSG;
    sections[8].offset = current_offset;
    sections[8].size = sizeof(struct _psg);
    sections[8].flags = 0;
    current_offset += sections[8].size;

    // Section 9: SRAM battery-backed (64KB)
    sections[9].section_id = SECTION_SRAM;
    sections[9].offset = current_offset;
    sections[9].size = 64 * 1024;
    sections[9].flags = 0;
    current_offset += sections[9].size;

    // Write header
    Write_Header(f, frameNumber);

    // Write section table
    for (int i = 0; i < NUM_SECTIONS; i++)
    {
        Write_Section_Entry(f, sections[i]);
    }

    // Write end marker
    Write_End_Marker(f);

    // Write section data

    // Section 0: 68000 RAM (direct copy)
    fwrite(Ram_68k, 1, 64 * 1024, f);

    // Section 1: 68000 Registers (collect and write)
    unsigned char m68k_regs[72];
    Collect_M68K_Registers(m68k_regs);
    fwrite(m68k_regs, 1, 72, f);

    // Section 2: VDP VRAM (direct copy)
    fwrite(VRam, 1, 64 * 1024, f);

    // Section 3: VDP CRAM (write as little-endian shorts)
    for (int i = 0; i < 64; i++)
    {
        unsigned short color = CRam[i];
        unsigned char bytes[2];
        bytes[0] = (color >> 0) & 0xFF;
        bytes[1] = (color >> 8) & 0xFF;
        fwrite(bytes, 1, 2, f);
    }

    // Section 4: VDP VSRAM (write 80 bytes)
    fwrite(VSRam, 1, 80, f);

    // Section 5: VDP Registers (collect and write)
    unsigned char vdp_regs[24];
    Collect_VDP_Registers(vdp_regs);
    fwrite(vdp_regs, 1, 24, f);

    // Section 6: Z80 RAM (direct copy)
    fwrite(Ram_Z80, 1, 8 * 1024, f);

    // Section 7: YM2612 (use built-in save function)
    unsigned char ym2612_state[0x14d0];
    YM2612_Save_Full(ym2612_state);
    fwrite(ym2612_state, 1, 0x14d0, f);

    // Section 8: PSG (direct struct copy)
    fwrite(&PSG, 1, sizeof(struct _psg), f);

    // Section 9: SRAM (direct copy)
    fwrite(SRAM, 1, 64 * 1024, f);

    fclose(f);
    return 1;
}

int StateDump_DumpStateToFile(const char* directory, const char* basename)
{
    char filename[1280];
    sprintf(filename, "%s\\%s.genstate", directory, basename);

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        // Failed to open file
        return 0;
    }

    // Use frame number 0 since we're using custom naming
    int frameNumber = 0;

    // Prepare sections
    const int NUM_SECTIONS = 10;
    SectionEntry sections[NUM_SECTIONS];

    // Calculate section offsets
    int header_size = 64;
    int section_table_size = (NUM_SECTIONS + 1) * 16;
    int current_offset = header_size + section_table_size;

    // Section 0: 68000 RAM
    sections[0].section_id = SECTION_M68K_RAM;
    sections[0].offset = current_offset;
    sections[0].size = 64 * 1024;
    sections[0].flags = 0;
    current_offset += sections[0].size;

    // Section 1: 68000 Registers
    sections[1].section_id = SECTION_M68K_REGS;
    sections[1].offset = current_offset;
    sections[1].size = 72;
    sections[1].flags = 0;
    current_offset += sections[1].size;

    // Section 2: VDP VRAM
    sections[2].section_id = SECTION_VDP_VRAM;
    sections[2].offset = current_offset;
    sections[2].size = 64 * 1024;
    sections[2].flags = 0;
    current_offset += sections[2].size;

    // Section 3: VDP CRAM
    sections[3].section_id = SECTION_VDP_CRAM;
    sections[3].offset = current_offset;
    sections[3].size = 128;
    sections[3].flags = 0;
    current_offset += sections[3].size;

    // Section 4: VDP VSRAM
    sections[4].section_id = SECTION_VDP_VSRAM;
    sections[4].offset = current_offset;
    sections[4].size = 80;
    sections[4].flags = 0;
    current_offset += sections[4].size;

    // Section 5: VDP Registers
    sections[5].section_id = SECTION_VDP_REGS;
    sections[5].offset = current_offset;
    sections[5].size = 24;
    sections[5].flags = 0;
    current_offset += sections[5].size;

    // Section 6: Z80 RAM
    sections[6].section_id = SECTION_Z80_RAM;
    sections[6].offset = current_offset;
    sections[6].size = 8 * 1024;
    sections[6].flags = 0;
    current_offset += sections[6].size;

    // Section 7: YM2612 FM sound chip
    sections[7].section_id = SECTION_YM2612;
    sections[7].offset = current_offset;
    sections[7].size = 0x14d0;
    sections[7].flags = 0;
    current_offset += sections[7].size;

    // Section 8: PSG sound generator
    sections[8].section_id = SECTION_PSG;
    sections[8].offset = current_offset;
    sections[8].size = sizeof(struct _psg);
    sections[8].flags = 0;
    current_offset += sections[8].size;

    // Section 9: SRAM battery-backed
    sections[9].section_id = SECTION_SRAM;
    sections[9].offset = current_offset;
    sections[9].size = 64 * 1024;
    sections[9].flags = 0;
    current_offset += sections[9].size;

    // Write header
    Write_Header(f, frameNumber);

    // Write section table
    for (int i = 0; i < NUM_SECTIONS; i++)
    {
        Write_Section_Entry(f, sections[i]);
    }

    // Write end marker
    Write_End_Marker(f);

    // Write section data
    fwrite(Ram_68k, 1, 64 * 1024, f);

    unsigned char m68k_regs[72];
    Collect_M68K_Registers(m68k_regs);
    fwrite(m68k_regs, 1, 72, f);

    fwrite(VRam, 1, 64 * 1024, f);

    for (int i = 0; i < 64; i++)
    {
        unsigned short color = CRam[i];
        unsigned char bytes[2];
        bytes[0] = (color >> 0) & 0xFF;
        bytes[1] = (color >> 8) & 0xFF;
        fwrite(bytes, 1, 2, f);
    }

    fwrite(VSRam, 1, 80, f);

    unsigned char vdp_regs[24];
    Collect_VDP_Registers(vdp_regs);
    fwrite(vdp_regs, 1, 24, f);

    // Z80 RAM
    fwrite(Ram_Z80, 1, 8 * 1024, f);

    // YM2612
    unsigned char ym2612_state[0x14d0];
    YM2612_Save_Full(ym2612_state);
    fwrite(ym2612_state, 1, 0x14d0, f);

    // PSG
    fwrite(&PSG, 1, sizeof(struct _psg), f);

    // SRAM
    fwrite(SRAM, 1, 64 * 1024, f);

    fclose(f);
    return 1;
}
