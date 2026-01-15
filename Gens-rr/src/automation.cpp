// Automation module for ROM analysis via screenshot comparison
// Supports two modes:
// 1. Record mode: saves screenshots to ScreenshotDir
// 2. Compare mode: loads from ReferenceDir, compares, saves diffs to ScreenshotDir

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "automation.h"
#include "state_dump.h"
#include "gens.h"
#include "G_main.h"
#include "G_ddraw.h"
#include "scrshot.h"
#include "png.h"
#include "drawutil.h"
#include "movie.h"
#include "vdp_io.h"
#include "Mem_M68k.h"
#include "Cpu_68k.h"
#include "Mem_Z80.h"
#include "ym2612.h"
#include "psg.h"

// External function from scrshot.cpp
extern bool write_png(void* data, int X, int Y, FILE* fp);

// External memory/state buffers (not declared in headers)
extern unsigned char Ram_68k[64 * 1024];
extern unsigned char Ram_Z80[8 * 1024];
extern unsigned char SRAM[64 * 1024];
// Note: VRam, CRam, VSRam are declared in vdp_io.h

// Global variables
int ScreenshotInterval = 0;
int MaxFrames = 0;
int MaxDiffs = 10;
int DiffCount = 0;
int MaxMemoryDiffs = 10;
int MemoryDiffCount = 0;
char ScreenshotDir[1024] = ".";
char ReferenceDir[1024] = "";
unsigned char DiffColor[4] = {255, 0, 255, 255};  // BGRA: Pink (magenta) by default
int CompareStateDumpsMode = 0;

// Internal buffer for reference image (320x240 max, BGRA = 4 bytes per pixel)
static unsigned char RefBuffer[320 * 240 * 4];
static unsigned char CurrentBuffer[320 * 240 * 4];
static unsigned char DiffBuffer[320 * 240 * 4];

void Automation_Init()
{
    ScreenshotInterval = 0;
    MaxFrames = 0;
    MaxDiffs = 10;
    DiffCount = 0;
    MaxMemoryDiffs = 10;
    MemoryDiffCount = 0;
    strcpy(ScreenshotDir, ".");
    ReferenceDir[0] = '\0';

    // Initialize state dump module
    StateDump_Init();
}

void Automation_Reset()
{
    DiffCount = 0;
    MemoryDiffCount = 0;
}

// Write current frame to BGRA buffer (based on WriteFrame from scrshot.cpp)
// Buffer is filled bottom-to-top (BMP order) - write_png will flip it
static void WriteFrameToBGRA(void* Screen, unsigned char* Dest, int mode, int Hmode, int Vmode, int X, int Y)
{
    int i, j;
    unsigned int tmp;
    unsigned char *Src = (unsigned char *)(Screen);

    int srcWidth = Hmode ? 320 : 256;
    int srcHeight = Vmode ? 240 : 224;
    int bytesPerPixel = (mode & 2) ? 4 : 2;

    // MD_Screen layout: 336 pixels wide, starting at offset 8
    // Start from bottom row of screen (row srcHeight-1)
    Src += (336 * (srcHeight - 1) + 8) * bytesPerPixel;

    // Write bottom row of screen to beginning of buffer (BMP order)
    // write_png will flip rows when writing PNG
    for(j = 0; j < srcHeight; j++)
    {
        unsigned char* dstPtr = Dest + j * srcWidth * 4;  // Sequential rows in buffer

        for(i = 0; i < srcWidth; i++)
        {
            // Read pixel based on mode
            if(mode & 2) // 32-bit
            {
                tmp = *(unsigned int*)&(Src[4 * i]);
            }
            else if(!(mode & 1)) // 16-bit 565
            {
                unsigned short pix = Src[2 * i] + (Src[2 * i + 1] << 8);
                tmp = DrawUtil::Pix16To32((pix16)pix) | 0xFF000000;
            }
            else // 16-bit 555
            {
                unsigned short pix = Src[2 * i] + (Src[2 * i + 1] << 8);
                tmp = DrawUtil::Pix15To32((pix15)pix) | 0xFF000000;
            }

            // Write BGRA
            dstPtr[4 * i + 0] = (tmp >>  0) & 0xFF; // B
            dstPtr[4 * i + 1] = (tmp >>  8) & 0xFF; // G
            dstPtr[4 * i + 2] = (tmp >> 16) & 0xFF; // R
            dstPtr[4 * i + 3] = 0xFF;               // A (opaque)
        }

        Src -= 336 * bytesPerPixel; // Move to previous row (going up on screen)
    }
}

int Save_Shot_To_File(void* screen, int mode, int Hmode, int Vmode, const char* filename)
{
    if (!screen) return 0;

    int X = Hmode ? 320 : 256;
    int Y = Vmode ? 240 : 224;

    // Allocate buffer for image data (BGRA = 4 bytes per pixel)
    int bufSize = X * Y * 4;
    unsigned char* Dest = (unsigned char*)malloc(bufSize);
    if (!Dest) return 0;
    memset(Dest, 0, bufSize);

    // Write frame to buffer (BGRA format, 32-bit)
    WriteFrameToBGRA(screen, Dest, mode, Hmode, Vmode, X, Y);

    // Save as PNG
    FILE* fp = fopen(filename, "wb");
    if (!fp)
    {
        free(Dest);
        return 0;
    }

    // Use write_png from scrshot.cpp
    bool result = write_png(Dest, X, Y, fp);

    fclose(fp);
    free(Dest);

    return result ? 1 : 0;
}

bool Load_PNG(const char* path, unsigned char* buffer, int bufferSize, int* width, int* height)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) return false;

    // Check PNG signature
    unsigned char header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
    {
        fclose(fp);
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    // Transform to BGRA
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);

    png_set_bgr(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    // Check buffer size
    int rowBytes = png_get_rowbytes(png_ptr, info_ptr);
    if (rowBytes * (*height) > bufferSize)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return false;
    }

    // Read image (bottom-up for our coordinate system)
    for (int y = *height - 1; y >= 0; y--)
    {
        png_read_row(png_ptr, buffer + y * rowBytes, NULL);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    return true;
}

bool Compare_With_Reference(void* screen, int mode, int Hmode, int Vmode, const char* refPath)
{
    int X = Hmode ? 320 : 256;
    int Y = Vmode ? 240 : 224;

    // Load reference PNG
    int refWidth, refHeight;
    if (!Load_PNG(refPath, RefBuffer, sizeof(RefBuffer), &refWidth, &refHeight))
    {
        // Reference file not found - treat as difference
        return false;
    }

    // Check dimensions match
    if (refWidth != X || refHeight != Y)
    {
        return false;
    }

    // Write current frame to buffer
    WriteFrameToBGRA(screen, CurrentBuffer, mode, Hmode, Vmode, X, Y);

    // Compare buffers and build diff image
    // Start with copy of reference, then overlay diff color on differing pixels
    int pixelCount = X * Y;
    bool hasDiff = false;
    memcpy(DiffBuffer, RefBuffer, pixelCount * 4);

    for (int i = 0; i < pixelCount; i++)
    {
        // Compare BGR only (skip alpha at offset +3)
        if (CurrentBuffer[i*4+0] != RefBuffer[i*4+0] ||  // B
            CurrentBuffer[i*4+1] != RefBuffer[i*4+1] ||  // G
            CurrentBuffer[i*4+2] != RefBuffer[i*4+2])    // R
        {
            hasDiff = true;
            // Overlay diff color on this pixel
            DiffBuffer[i*4+0] = DiffColor[0];  // B
            DiffBuffer[i*4+1] = DiffColor[1];  // G
            DiffBuffer[i*4+2] = DiffColor[2];  // R
            DiffBuffer[i*4+3] = DiffColor[3];  // A
        }
    }

    return !hasDiff;  // true if screens match
}

// Save diff visualization image (reference with diff pixels highlighted)
bool Save_Diff_Image(int X, int Y, const char* filename)
{
    FILE* fp = fopen(filename, "wb");
    if (!fp) return false;

    bool result = write_png(DiffBuffer, X, Y, fp);
    fclose(fp);
    return result;
}

// Section name lookup for CSV output
static const char* GetSectionName(unsigned int section_id)
{
    switch (section_id)
    {
        case SECTION_M68K_RAM:  return "M68K_RAM";
        case SECTION_M68K_REGS: return "M68K_REGS";
        case SECTION_VDP_VRAM:  return "VDP_VRAM";
        case SECTION_VDP_CRAM:  return "VDP_CRAM";
        case SECTION_VDP_VSRAM: return "VDP_VSRAM";
        case SECTION_VDP_REGS:  return "VDP_REGS";
        case SECTION_Z80_RAM:   return "Z80_RAM";
        case SECTION_Z80_REGS:  return "Z80_REGS";
        case SECTION_YM2612:    return "YM2612";
        case SECTION_PSG:       return "PSG";
        case SECTION_SRAM:      return "SRAM";
        default: return "UNKNOWN";
    }
}

// Compare section data and write diffs to file
// Returns number of differing bytes
static int Compare_Section_And_Write(FILE* fp, const char* sectionName,
                                     unsigned char* refData, unsigned char* currentData, int size)
{
    int diffCount = 0;
    for (int i = 0; i < size; i++)
    {
        if (refData[i] != currentData[i])
        {
            int diff = (int)currentData[i] - (int)refData[i];
            fprintf(fp, "%s,0x%04X,0x%02X,0x%02X,%d\n",
                    sectionName, i, refData[i], currentData[i], diff);
            diffCount++;
        }
    }
    return diffCount;
}

// Read little-endian 32-bit integer from buffer
static unsigned int Read_LE_U32(unsigned char* buf)
{
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

// Collect current M68K registers into buffer (same format as state_dump.cpp)
static void Collect_Current_M68K_Regs(unsigned char* buffer)
{
    unsigned char* ptr = buffer;

    // D0-D7
    for (int i = 0; i < 8; i++)
    {
        unsigned int val = Context_68K.dreg[i];
        ptr[0] = (val >> 0) & 0xFF;
        ptr[1] = (val >> 8) & 0xFF;
        ptr[2] = (val >> 16) & 0xFF;
        ptr[3] = (val >> 24) & 0xFF;
        ptr += 4;
    }

    // A0-A7
    for (int i = 0; i < 8; i++)
    {
        unsigned int val = Context_68K.areg[i];
        ptr[0] = (val >> 0) & 0xFF;
        ptr[1] = (val >> 8) & 0xFF;
        ptr[2] = (val >> 16) & 0xFF;
        ptr[3] = (val >> 24) & 0xFF;
        ptr += 4;
    }

    // PC
    unsigned int pc = Context_68K.pc;
    ptr[0] = (pc >> 0) & 0xFF;
    ptr[1] = (pc >> 8) & 0xFF;
    ptr[2] = (pc >> 16) & 0xFF;
    ptr[3] = (pc >> 24) & 0xFF;
    ptr += 4;

    // SR
    unsigned int sr = Context_68K.sr;
    ptr[0] = (sr >> 0) & 0xFF;
    ptr[1] = (sr >> 8) & 0xFF;
    ptr[2] = 0;
    ptr[3] = 0;
}

// Collect current VDP registers into buffer
static void Collect_Current_VDP_Regs(unsigned char* buffer)
{
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

// Collect current CRAM into buffer (little-endian shorts)
static void Collect_Current_CRAM(unsigned char* buffer)
{
    for (int i = 0; i < 64; i++)
    {
        unsigned short color = CRam[i];
        buffer[i * 2 + 0] = (color >> 0) & 0xFF;
        buffer[i * 2 + 1] = (color >> 8) & 0xFF;
    }
}

// Compare full genstate file with current emulator state
// Writes all diffs to CSV file with section information
// Returns total number of differing bytes across all sections
int Compare_Full_State_And_Save_Diff(const char* refStatePath, const char* directory, const char* basename)
{
    FILE* refFile = fopen(refStatePath, "rb");
    if (!refFile) return 0;

    // Get file size
    fseek(refFile, 0, SEEK_END);
    long fileSize = ftell(refFile);
    fseek(refFile, 0, SEEK_SET);

    // Read entire file
    unsigned char* fileData = new unsigned char[fileSize];
    if (fread(fileData, 1, fileSize, refFile) != (size_t)fileSize)
    {
        delete[] fileData;
        fclose(refFile);
        return 0;
    }
    fclose(refFile);

    // Open diff output file
    char diffFilename[1280];
    sprintf(diffFilename, "%s\\%s_memdiff.csv", directory, basename);
    FILE* diffFile = fopen(diffFilename, "w");
    if (!diffFile)
    {
        delete[] fileData;
        return 0;
    }

    // Write CSV header
    fprintf(diffFile, "section,address,expected,actual,diff\n");

    int totalDiffs = 0;

    // Parse section table (starts at offset 64, after header)
    unsigned char* sectionTable = fileData + 64;
    int sectionIndex = 0;

    while (true)
    {
        unsigned char* entry = sectionTable + sectionIndex * 16;
        unsigned int section_id = Read_LE_U32(entry);
        unsigned int offset = Read_LE_U32(entry + 4);
        unsigned int size = Read_LE_U32(entry + 8);

        // End marker (all zeros)
        if (section_id == 0 && offset == 0 && size == 0) break;

        // Get reference data pointer
        unsigned char* refData = fileData + offset;
        const char* sectionName = GetSectionName(section_id);

        // Get current data based on section type
        unsigned char* currentData = NULL;
        unsigned char tempBuffer[256]; // For registers
        static unsigned char ym2612Buffer[0x14d0]; // YM2612 state buffer
        static unsigned char psgBuffer[sizeof(struct _psg)]; // PSG state buffer

        switch (section_id)
        {
            case SECTION_M68K_RAM:
                currentData = Ram_68k;
                break;

            case SECTION_M68K_REGS:
                Collect_Current_M68K_Regs(tempBuffer);
                currentData = tempBuffer;
                break;

            case SECTION_VDP_VRAM:
                currentData = VRam;
                break;

            case SECTION_VDP_CRAM:
                Collect_Current_CRAM(tempBuffer);
                currentData = tempBuffer;
                break;

            case SECTION_VDP_VSRAM:
                currentData = VSRam;
                break;

            case SECTION_VDP_REGS:
                Collect_Current_VDP_Regs(tempBuffer);
                currentData = tempBuffer;
                break;

            case SECTION_Z80_RAM:
                currentData = Ram_Z80;
                break;

            case SECTION_YM2612:
                YM2612_Save_Full(ym2612Buffer);
                currentData = ym2612Buffer;
                break;

            case SECTION_PSG:
                memcpy(psgBuffer, &PSG, sizeof(struct _psg));
                currentData = psgBuffer;
                break;

            case SECTION_SRAM:
                currentData = SRAM;
                break;

            // Skip Z80 registers for now (complex structure)
            default:
                currentData = NULL;
                break;
        }

        // Compare and write diffs
        if (currentData)
        {
            totalDiffs += Compare_Section_And_Write(diffFile, sectionName, refData, currentData, size);
        }

        sectionIndex++;
        if (sectionIndex > 20) break; // Safety limit
    }

    fclose(diffFile);
    delete[] fileData;

    // Delete empty diff file
    if (totalDiffs == 0)
    {
        remove(diffFilename);
    }

    return totalDiffs;
}

void Automation_OnFrame(int frameCount, void* screen, int mode, int Hmode, int Vmode)
{
    // Process state dumps (independent of screenshot automation)
    StateDump_OnFrame(frameCount);

    // Skip if screenshot automation disabled
    if (ScreenshotInterval <= 0) return;

    // Check max frames limit first (takes priority over movie end)
    if (MaxFrames > 0 && frameCount >= MaxFrames)
    {
        // Post close message to end emulation
        PostMessage(HWnd, WM_CLOSE, 0, 0);
        return;
    }

    // If no max frames limit, close when movie finishes
    if (MaxFrames == 0 && MainMovie.Status == MOVIE_FINISHED)
    {
        PostMessage(HWnd, WM_CLOSE, 0, 0);
        return;
    }

    // Only process on interval frames
    if (frameCount % ScreenshotInterval != 0) return;

    // Build screenshot filename (frame number as name)
    char filename[1024];
    sprintf(filename, "%s\\%06d.png", ScreenshotDir, frameCount);

    // Build basename for state dumps (without extension)
    char basename[32];
    sprintf(basename, "%06d", frameCount);

    if (ReferenceDir[0] == '\0')
    {
        // RECORD MODE: save screenshot (and optionally state dump)
        Save_Shot_To_File(screen, mode, Hmode, Vmode, filename);

        // If state dump mode is enabled, save state dump alongside screenshot
        if (StateDumpWithScreenshots)
        {
            StateDump_DumpStateToFile(ScreenshotDir, basename);
        }
    }
    else
    {
        // COMPARE MODE: compare BOTH screenshots AND memory
        // Two independent counters: DiffCount for screenshots, MemoryDiffCount for memory
        // Terminate when EITHER counter reaches its threshold

        bool screenshotDiff = false;
        bool memoryDiff = false;

        // 1. SCREENSHOT COMPARISON
        char refPath[1024];
        sprintf(refPath, "%s\\%06d.png", ReferenceDir, frameCount);

        if (!Compare_With_Reference(screen, mode, Hmode, Vmode, refPath))
        {
            screenshotDiff = true;

            // Save current screenshot
            Save_Shot_To_File(screen, mode, Hmode, Vmode, filename);

            // Save diff visualization (reference with diff pixels highlighted)
            char diffFilename[1024];
            sprintf(diffFilename, "%s\\%06d_diff.png", ScreenshotDir, frameCount);
            int X = Hmode ? 320 : 256;
            int Y = Vmode ? 240 : 224;
            Save_Diff_Image(X, Y, diffFilename);

            DiffCount++;
        }

        // 2. FULL STATE COMPARISON (all sections: RAM, VRAM, CRAM, registers, etc.)
        char refStatePath[1024];
        sprintf(refStatePath, "%s\\%06d.genstate", ReferenceDir, frameCount);

        // Compare full state and save diff CSV (section, address, expected, actual)
        int stateDiffs = Compare_Full_State_And_Save_Diff(refStatePath, ScreenshotDir, basename);

        if (stateDiffs > 0)
        {
            memoryDiff = true;
            MemoryDiffCount++;

            // Save current state dump for full state analysis
            StateDump_DumpStateToFile(ScreenshotDir, basename);

            // Also save screenshot for visual reference (if not already saved by screenshot diff)
            if (!screenshotDiff)
            {
                Save_Shot_To_File(screen, mode, Hmode, Vmode, filename);
            }
        }

        // If screenshot diff but no memory diff, still save state dump for analysis
        if (screenshotDiff && !memoryDiff)
        {
            StateDump_DumpStateToFile(ScreenshotDir, basename);
        }

        // Check if either threshold reached - terminate if so
        if ((MaxDiffs > 0 && DiffCount >= MaxDiffs) ||
            (MaxMemoryDiffs > 0 && MemoryDiffCount >= MaxMemoryDiffs))
        {
            // Exceeded diff limit - early exit
            PostMessage(HWnd, WM_CLOSE, 0, 0);
        }
    }
}
