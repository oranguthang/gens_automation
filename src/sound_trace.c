#include <stdio.h>
#include <stdlib.h>

#include "sound_trace.h"

extern long unsigned int FrameCount;

static FILE* SoundTraceFile = NULL;
static int SoundTraceStart = 0;
static int SoundTraceEnd = 0;
static unsigned long SoundTraceSequence = 0;
static unsigned char SoundTraceYMAddress[2] = {0, 0};

static int SoundTrace_InRange(void)
{
    return SoundTraceFile &&
        FrameCount >= (unsigned long)SoundTraceStart &&
        (!SoundTraceEnd || FrameCount <= (unsigned long)SoundTraceEnd);
}

static void SoundTrace_Close(void)
{
    if (SoundTraceFile)
    {
        fclose(SoundTraceFile);
        SoundTraceFile = NULL;
    }
}

int SoundTrace_Open(const char* path, int startFrame, int endFrame)
{
    SoundTrace_Close();
    SoundTraceFile = fopen(path, "w");
    if (!SoundTraceFile) return 0;
    SoundTraceStart = startFrame < 0 ? 0 : startFrame;
    SoundTraceEnd = endFrame < 0 ? 0 : endFrame;
    SoundTraceSequence = 0;
    fprintf(SoundTraceFile, "sequence,frame,chip,port,address,value\n");
    fflush(SoundTraceFile);
    atexit(SoundTrace_Close);
    return 1;
}

void SoundTrace_YMWrite(unsigned char port, unsigned char data)
{
    unsigned int bank = (port >> 1) & 1;
    if (!(port & 1))
    {
        SoundTraceYMAddress[bank] = data;
        return;
    }
    if (!SoundTrace_InRange()) return;
    fprintf(SoundTraceFile, "%lu,%lu,YM2612,%u,%02X,%02X\n",
        SoundTraceSequence++, FrameCount, bank,
        SoundTraceYMAddress[bank], data);
}

void SoundTrace_PSGWrite(unsigned char data)
{
    if (!SoundTrace_InRange()) return;
    fprintf(SoundTraceFile, "%lu,%lu,SN76489,0,00,%02X\n",
        SoundTraceSequence++, FrameCount, data);
}
