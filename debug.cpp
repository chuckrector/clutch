#include "memory.h"

static b32 GlobalLogInitialized = false;
static char *GlobalLogBuffer;
static umm GlobalLogBufferPendingSize = 0;
static b32 GlobalLogBufferPendingFlushed = false;

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

static void
InitLog(wchar_t *LogPath)
{
    // printf("InitLog %S\n", LogPath);
    if(FileExists(LogPath)) DeleteFileW(LogPath);
    LogHandle = CreateFileW(LogPath, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);
    GlobalLogInitialized = true;
}

// NOTE(chuck): Logging can occur before the log has been initialized.  It will buffer.  After
// the log has been initialized (and therefore the log file has been opened), the next call will
// flush the buffer.
static void
Log(char *Format, ...)
{
    // printf("Log format %s\n", Format);
    if(!GlobalLogBuffer)
    {
        GlobalLogBuffer = PushArray(Megabytes(4), char);        
    }

    DWORD BytesWritten;
    if(GlobalLogInitialized && !GlobalLogBufferPendingFlushed)
    {
        if(GlobalLogBufferPendingSize)
        {
            if(!WriteFile(LogHandle, GlobalLogBuffer, (DWORD)GlobalLogBufferPendingSize, &BytesWritten, 0))
            {
                Quit("There was a problem writing pending logs to the log file.\n"
                     "%d bytes were written while %d were expected to be written.\n",
                     BytesWritten, GlobalLogBufferPendingSize);
            }
        }
        GlobalLogBufferPendingSize = 0;
        GlobalLogBufferPendingFlushed = true;
    }

    va_list Args;
    va_start(Args, Format);
    umm FormattedStringLength = FormatStringList(4096, GlobalLogBuffer + GlobalLogBufferPendingSize, Format, Args);
    va_end(Args);

    if(GlobalLogInitialized)
    {
        if(!WriteFile(LogHandle, GlobalLogBuffer, (DWORD)FormattedStringLength, &BytesWritten, 0))
        {
            Quit("There was a problem writing to the log file.  "
                "%d bytes were written while %d were expected to be written.\n",
                BytesWritten, FormattedStringLength);
        }
        FlushFileBuffers(LogHandle); // NOTE(chuck): To avoid having to close the log at the end of the program.
    }
    else
    {
        GlobalLogBufferPendingSize += FormattedStringLength;
    }
}
