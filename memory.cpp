#include "memory.h"

static char GlobalMemory[MEMORY_LIMIT] = {};
static umm GlobalMemoryUsed = 0;

static char *
PushSize(umm Size)
{
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

