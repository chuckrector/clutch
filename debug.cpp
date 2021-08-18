#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }

static void
Quit(char *Format, ...)
{
    char Buffer[4096];
    va_list Args;
    va_start(Args, Format);
    FormatStringList(4096, Buffer, Format, Args);
    va_end(Args);

    Printf(STD_ERROR_HANDLE, Buffer);

    DWORD ErrorCode = GetLastError();
    if(ErrorCode)
    {
        char ErrorMessageBuffer[4096];
        DWORD BytesWritten = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, ErrorCode, 0, ErrorMessageBuffer, 4096, 0);
        Printf(STD_ERROR_HANDLE, "\nSystem error code %d: %s\n", ErrorCode, ErrorMessageBuffer);
        ExitProcess(ErrorCode);
    }
    else
    {
        ExitProcess(1);
    }
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
    char Buffer[4096];
    va_list Args;
    va_start(Args, Format);
    umm BufferLength = FormatStringList(4096, Buffer, Format, Args);
    va_end(Args);

    DWORD BytesWritten;
    WriteFile(LogHandle, Buffer, (DWORD)BufferLength, &BytesWritten, 0);
    FlushFileBuffers(LogHandle); // NOTE(chuck): To avoid having to close the log at the end of the program.
}

