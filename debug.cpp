#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }

static void
Quit(char *Format, ...)
{
    char Buffer[1024];
    va_list Args;
    va_start(Args, Format);
    FormatStringList(1024, Buffer, Format, Args);
    va_end(Args);

    Printf(STD_ERROR_HANDLE, Buffer);
    ExitProcess(1);
}

static void
InitLog(wchar_t *LogPath)
{
    if(FileExists(LogPath)) DeleteFileW(LogPath);
    LogHandle = CreateFileW(LogPath, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
}

static void
Log(char *Format, ...)
{
    char Buffer[1024];
    va_list Args;
    va_start(Args, Format);
    umm BufferLength = FormatStringList(1024, Buffer, Format, Args);
    va_end(Args);

    DWORD BytesWritten;
    WriteFile(LogHandle, Buffer, (DWORD)BufferLength, &BytesWritten, 0);
    FlushFileBuffers(LogHandle); // NOTE(chuck): To avoid having to close the log at the end of the program.
}

