#ifndef SOUND_TRACE_H
#define SOUND_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

int SoundTrace_Open(const char* path, int startFrame, int endFrame);
void SoundTrace_YMWrite(unsigned char port, unsigned char data);
void SoundTrace_PSGWrite(unsigned char data);

#ifdef __cplusplus
}
#endif

#endif
