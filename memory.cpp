// NOTE(chuck): Memory management and string functions

#define Kilobytes(N) ((N)*1024)
#define Megabytes(N) Kilobytes((N)*1024)
#define Gigabytes(N) Megabytes((N)*1024)
#define Terabytes(N) Gigabytes((N)*1024)

#define MEMORY_LIMIT Megabytes(64)

static char GlobalMemory[MEMORY_LIMIT] = {};
static umm GlobalMemoryUsed = 0;

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define PushStruct(type) (type *)PushSize(sizeof(type))
#define PushArray(Count, type, ...) (type *)PushSize((Count)*sizeof(type), ## __VA_ARGS__)

#define MemCurrent(type) (type *)((char *)GlobalMemory + GlobalMemoryUsed)
#define MemSkip(Count, type) GlobalMemoryUsed += ((Count)*sizeof(type))

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

