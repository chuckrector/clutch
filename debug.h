#if !defined(DEBUG_H)

#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }


static void Quit(char *Format, ...);
static void InitLog(wchar_t *LogPath);
static void Log(char *Format, ...);

static HANDLE LogHandle;

#define DEBUG_H
#endif