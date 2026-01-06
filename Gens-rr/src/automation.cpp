// Automation module for ROM analysis via screenshot comparison
// Supports two modes:
// 1. Record mode: saves screenshots to ScreenshotDir
// 2. Compare mode: loads from ReferenceDir, compares, saves diffs to ScreenshotDir

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "automation.h"
#include "gens.h"
#include "G_main.h"
#include "G_ddraw.h"
#include "scrshot.h"
#include "png.h"
#include "drawutil.h"
#include "movie.h"
#include "vdp_io.h"

// External function from scrshot.cpp
extern bool write_png(void* data, int X, int Y, FILE* fp);

// Global variables
int ScreenshotInterval = 0;
int MaxFrames = 0;
int MaxDiffs = 10;
int DiffCount = 0;
char ScreenshotDir[1024] = ".";
char ReferenceDir[1024] = "";

// Internal buffer for reference image (320x240 max, BGRA = 4 bytes per pixel)
static unsigned char RefBuffer[320 * 240 * 4];
static unsigned char CurrentBuffer[320 * 240 * 4];

void Automation_Init()
{
    ScreenshotInterval = 0;
    MaxFrames = 0;
    MaxDiffs = 10;
    DiffCount = 0;
    strcpy(ScreenshotDir, ".");
    ReferenceDir[0] = '\0';
}

void Automation_Reset()
{
    DiffCount = 0;
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

    // Compare buffers (BGRA, ignore alpha)
    int pixelCount = X * Y;
    for (int i = 0; i < pixelCount; i++)
    {
        // Compare BGR only (skip alpha at offset +3)
        if (CurrentBuffer[i*4+0] != RefBuffer[i*4+0] ||  // B
            CurrentBuffer[i*4+1] != RefBuffer[i*4+1] ||  // G
            CurrentBuffer[i*4+2] != RefBuffer[i*4+2])    // R
        {
            return false;  // Difference found
        }
    }

    return true;  // Screens match
}

void Automation_OnFrame(int frameCount, void* screen, int mode, int Hmode, int Vmode)
{
    // Skip if automation disabled
    if (ScreenshotInterval <= 0) return;

    // Only process on interval frames
    if (frameCount % ScreenshotInterval != 0) return;

    // Check max frames limit
    if (MaxFrames > 0 && frameCount >= MaxFrames)
    {
        // Post close message to end emulation
        PostMessage(HWnd, WM_CLOSE, 0, 0);
        return;
    }

    // Build screenshot filename (frame number as name)
    char filename[1024];
    sprintf(filename, "%s\\%06d.png", ScreenshotDir, frameCount);

    if (ReferenceDir[0] == '\0')
    {
        // RECORD MODE: just save screenshot
        Save_Shot_To_File(screen, mode, Hmode, Vmode, filename);
    }
    else
    {
        // COMPARE MODE: load reference and compare
        char refPath[1024];
        sprintf(refPath, "%s\\%06d.png", ReferenceDir, frameCount);

        if (!Compare_With_Reference(screen, mode, Hmode, Vmode, refPath))
        {
            // Difference found! Save current screenshot
            Save_Shot_To_File(screen, mode, Hmode, Vmode, filename);
            DiffCount++;

            // Check max diffs limit
            if (MaxDiffs > 0 && DiffCount >= MaxDiffs)
            {
                // Exceeded diff limit - early exit
                PostMessage(HWnd, WM_CLOSE, 0, 0);
            }
        }
    }
}
