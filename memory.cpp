#include "memory.h"

static char GlobalMemory[MEMORY_LIMIT] = {};
static umm GlobalMemoryUsed = 0;

static char *
PushSize(umm Size)
{
    /* NOTE(chuck): I just learned that pointers to misaligned memory addresses can
       cause failures or even crashes.  For example, this was causing QueryDosDeviceW()
       to fail with "998 Invalid access to memory location". Align everything to
       16-bytes because that's the x64 stack alignment constraint and seems like it
       will make it to where I mostly don't have to worry about this for a good long
       while, even if I start doing wide ops. */
    Size = ((Size + 15) & ~15);
    Assert((GlobalMemoryUsed + Size) <= MEMORY_LIMIT);
    char *Result = MemCurrent(char);
    char *P = Result;
    char *End = Result + Size;
    while(P < End)
    {
        *P++ = 0;
    }
    GlobalMemoryUsed += Size;
    // printf("PushSize %d. Total used %Id\n", Size, GlobalMemoryUsed);
    return(Result);
}

static void
MemCopy(char *To, char *From, umm Count)
{
    while(Count--) *To++ = *From++;
}

static void
MemCopy(wchar_t *To, wchar_t *From, umm Count)
{
    while(Count--) *To++ = *From++;
}

static void
MemClear(void *Bytes, umm Count)
{
    char *P = (char *)Bytes;
    while(Count--) *P++ = 0;
}

