#include "memory.h"

static void
Quit(char *Format, ...)
{
    char *Buffer = PushArray(4096, char);
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

static b32 GlobalLogInitialized = false;

static void
InitLog(wchar_t *LogPath)
{
    if(FileExists(LogPath)) DeleteFileW(LogPath);
    LogHandle = CreateFileW(LogPath, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    GlobalLogInitialized = true;
}

static void
Log(char *Format, ...)
{
    Assert(GlobalLogInitialized);

    char *Buffer = PushArray(4096, char);
    va_list Args;
    va_start(Args, Format);
    umm BufferLength = FormatStringList(4096, Buffer, Format, Args);
    va_end(Args);

    DWORD BytesWritten;
    if(!WriteFile(LogHandle, Buffer, (DWORD)BufferLength, &BytesWritten, 0))
    {
        Quit("There was a problem writing to the log file.  %d bytes were written while %d were expected to be written.\n",
             BytesWritten, BufferLength);
    }
    FlushFileBuffers(LogHandle); // NOTE(chuck): To avoid having to close the log at the end of the program.
}
