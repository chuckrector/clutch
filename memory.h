#if !defined(MEMORY_H)

#define Kilobytes(N) ((N)*1024)
#define Megabytes(N) Kilobytes((N)*1024)
#define Gigabytes(N) Megabytes((N)*1024)
#define Terabytes(N) Gigabytes((N)*1024)

#define MEMORY_LIMIT Megabytes(64)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define PushStruct(type) (type *)PushSize(sizeof(type))
#define PushArray(Count, type) (type *)PushSize((Count)*sizeof(type))

#define MemCurrent(type) (type *)((char *)GlobalMemory + GlobalMemoryUsed)
#define MemSkip(Count, type) GlobalMemoryUsed += ((Count)*sizeof(type))

static char *PushSize(umm Size);

static void MemCopy(char *To, char *From, umm Count);
static void MemCopy(wchar_t *To, wchar_t *From, umm Count);
static void MemClear(void *Bytes, umm Count);

#define MEMORY_H
#endif