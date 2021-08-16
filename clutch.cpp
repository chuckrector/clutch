/*
NOTE(chuck): This code is inspired heavily by Handmade Hero https://handmadehero.org/
and its community.  If you're wondering why I've even bothered to handroll some of the
things in here, it's because I'm learning and trying out a lot of things. I'm interested
in getting more serious about becoming a better programmer and writing useful programs.
This is the third program I've been working on in the past 6mo or so along those lines.
*/
// TODO(chuck): Add timestamps, elapsed time, and total size in the log.

#include "clutch.h"

int
main()
{
    program_args ProgramArgs = GetProgramArgs();
    
    if(!((ProgramArgs.Count == 2) || (ProgramArgs.Count == 3)))
    {
        Printf(
            "Clutch v1.0 by Chuck Rector\n"
            "Create a portable cl.exe tailored to an existing program.\n\n"
            "Usage:\n"
            "  clutch <path to build script>\n"
            "  clutch <path to build script> <target directory>\n\n"
            "Examples:\n"
            "  clutch build\n"
            "  clutch C:\\wherever\\build.bat C:\\over_here\n"
            "  clutch make portable_cl\n\n"
            "The program must already successfully build with cl.exe.  The specific set of\n"
            "MSVC files needed to build the program will be captured.  The default target\n"
            "directory is the subdirectory `cl` in the current directory.  If the target\n"
            "directory contains any files or folders, they are deleted beforehand.\n\n"
            "Your build script's file I/O is watched:\n"
            "  - All .exe and .dll are copied into the target's root.\n"
            "    Note: Subdirectories relative to cl.exe are preserved in the target.\n"
            "          e.g. C:\\Program Files ... Hostx64\\x64\\cl.exe\n"
            "               C:\\Program Files ... Hostx64\\x64\\1033\\clui.dll\n"
            "               cl\\1033\\clui.dll is captured in the target.\n"
            "  - All .lib and .pdb are merged into the `lib` subdirectory.\n"
            "  - All .h are merged into the `include` subdirectory.\n");
        ExitProcess(1);
    }

    wchar_t *BuildScript = GetArg(&ProgramArgs, 1);
    wchar_t *TargetDirectory = L"cl";
    if(ProgramArgs.Count == 3)
    {
        TargetDirectory = GetArg(&ProgramArgs, 2);
    }

    wchar_t *CurrentDirectory = PushArray(1024, wchar_t);
    GetCurrentDirectoryW(1024, CurrentDirectory);
    int CurrentDirectoryLength = StringLength(CurrentDirectory);
    if(CurrentDirectory[CurrentDirectoryLength - 1] != '\\')
    {
        CurrentDirectory[CurrentDirectoryLength++] = '\\';
    }

    wchar_t *FullBuildScript = GetFullPath(BuildScript);
    smm FullBuildScriptLastSlashIndex = GetLastCharIndexInString(FullBuildScript, '\\');
    if(FullBuildScriptLastSlashIndex == -1)
    {
        Quit("Couldn't find the trailing backslash in the build script path: %S\n", FullBuildScript);
    }

    wchar_t *FullTargetDirectory = GetFullPath(TargetDirectory);

    int TotalFilesCopied = 0;
    int TotalHFilesCopied = 0;
    int TotalLibFilesCopied = 0;
    int TotalPDBFilesCopied = 0;
    int TotalDLLFilesCopied = 0;
    int TotalEXEFilesCopied = 0;
    int TotalDirectoriesCreated = 0;

    // NOTE(chuck): Target directory must exist prior to opening the log (which doubles as a manifest).
    TotalDirectoriesCreated += CreateDirectory(FullTargetDirectory);
    if(!TotalDirectoriesCreated)
    {
        // NOTE(chuck): Delete all files before opening log because log will hold a lock on the file
        // and cause any deletion attempts to fail.
        DeleteFilesRecursively(FullTargetDirectory);
    }

    // TODO(chuck): For reasons I don't yet understand, getting the volume list fails if I attempt to
    // do it after opening the log file.  Why??
    win32_volume_list VolumeList = Win32GetVolumeList();
    umm VolumeListingBufferSize = Kilobytes(4);
    char *VolumeListingBuffer = PushArray(VolumeListingBufferSize, char);
    {
        char *P = VolumeListingBuffer;
        umm BytesWritten = FormatString(VolumeListingBufferSize, P, "Volumes (%d)\n", VolumeList.Count);
        VolumeListingBufferSize -= BytesWritten;
        P += BytesWritten;
        for(umm Index = 0;
            Index < VolumeList.Count;
            ++Index)
        {
            win32_volume *Volume = VolumeList.Volume + Index;
            BytesWritten = FormatString(VolumeListingBufferSize, P, "    %S -> %S -> %S\n", Volume->GUID, Volume->DeviceName, Volume->Drive);
            VolumeListingBufferSize -= BytesWritten;
            P += BytesWritten;
        }
    }

    char *LogPathA = PushSize(1024);
    FormatString(1024, LogPathA, "%S\\clutch.log", FullTargetDirectory);
    InitLog(WidenChars(LogPathA));

    Log("Target %S\n", FullTargetDirectory);

    char *TargetIncludeDirectoryA = PushSize(1024);
    FormatString(1024, TargetIncludeDirectoryA, "%S\\include", FullTargetDirectory);
    wchar_t *TargetIncludeDirectory = WidenChars(TargetIncludeDirectoryA);

    char *TargetLibDirectoryA = PushSize(1024);
    FormatString(1024, TargetLibDirectoryA, "%S\\lib", FullTargetDirectory);
    wchar_t *TargetLibDirectory = WidenChars(TargetLibDirectoryA);

    TotalDirectoriesCreated += CreateDirectory(TargetIncludeDirectory);
    TotalDirectoriesCreated += CreateDirectory(TargetLibDirectory);

    process *ProcessFilterHead = PushStruct(process);
    process *ProcessFilterTail = ProcessFilterHead;
    int ProcessFilterCount = 0;

    // NOTE(chuck): Run build script and capture its stderr/stdout.  This is done for
    // debuggability if the build script fails.
    etw_event_trace *ETWEventTrace;
    {
        HANDLE ReadStdout;
        HANDLE WriteStdout;
        SECURITY_ATTRIBUTES SecurityAttributes = {};
        SecurityAttributes.nLength = sizeof(SecurityAttributes); 
        // NOTE(chuck): The child process won't be able to make use of these pipes unless
        // they have inheritance enabled.
        SecurityAttributes.bInheritHandle = 1;
        
        if(!CreatePipe(&ReadStdout, &WriteStdout, &SecurityAttributes, 0))
        {
            Quit("Failed to create stdout pipe.  Error code %d\n", GetLastError());
        }

        STARTUPINFOW StartupInfo = {};
        PROCESS_INFORMATION ProcessInfo = {};
        StartupInfo.cb = sizeof(STARTUPINFOW); 
        StartupInfo.hStdError = WriteStdout;
        StartupInfo.hStdOutput = WriteStdout;
        StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        
        wchar_t *WindowsDirectory = PushArray(1024, wchar_t);
        UINT WindowsDirectoryLength = GetWindowsDirectoryW(WindowsDirectory, 1024);
        if(!WindowsDirectoryLength)
        {
            Quit("Failed to get the Windows directory.  Error code %d\n", GetLastError());
        }

        wchar_t *CmdExe = PushArray(1024, wchar_t);
        CopyString(CmdExe, WindowsDirectory);
        CopyString(CmdExe + WindowsDirectoryLength, L"\\System32\\cmd.exe");
        char *CmdArgsA = PushSize(1024);
        FormatString(1024, CmdArgsA, "/c %S", FullBuildScript);
        wchar_t *CmdArgs = WidenChars(CmdArgsA);
        
        // NOTE(chuck): The child process won't be able to read stdout unless handle
        // inheritance is enabled when creating it.
        b32 InheritHandles = 1;
        b32 CreateProcessResult = CreateProcessW(CmdExe, CmdArgs, 0, 0, InheritHandles,
                                                 CREATE_SUSPENDED, 0, 0,
                                                 &StartupInfo, &ProcessInfo);
        if(!CreateProcessResult)
        {
            Quit("Failed to create process.  Error code %d\n", GetLastError());
        }

        Log("Build script %S %S\n", CmdExe, CmdArgs);
        Log("Windows directory: %S\n", WindowsDirectory);
        Log(VolumeListingBuffer);

        process *Process = ProcessFilterTail;
        ProcessFilterTail->Next = PushStruct(process);
        ProcessFilterTail = ProcessFilterTail->Next;
        ++ProcessFilterCount;

        Process->ImageFilename = PushArray(8, wchar_t);
        Process->CommandLine = PushArray(StringLength(CmdArgs) + 1, wchar_t);
        Process->ProcessID = ProcessInfo.dwProcessId;
        // Printf("  Root process PID %d\n", Process->ProcessID);
        MemCopy(Process->ImageFilename, L"cmd.exe", 8);
        MemCopy(Process->CommandLine, CmdArgs, StringLength(CmdArgs));

        // NOTE(chuck): Start the trace only after we've setup the main process ID for filtering.
        ETWEventTrace = ETWBeginTrace();
        ETWAddEventType(ETWEventTrace, ETWType_FileIO);
        ETWAddEventType(ETWEventTrace, ETWType_Process);

        ResumeThread(ProcessInfo.hThread);
        
        // NOTE(chuck): stdout must be closed before reading or we will hang.
        CloseHandle(WriteStdout);

        int StdoutBufferSize = Megabytes(4);
        char *StdoutBuffer = PushArray(StdoutBufferSize, char);
        char *StdoutBufferP = StdoutBuffer;
        int BytesRemaining = StdoutBufferSize;
        int TotalBytesRead = 0;
        b32 GotFullStdout = false;

        for(;;)
        {
            DWORD Read;
            b32 Success = ReadFile(ReadStdout, StdoutBufferP, BytesRemaining, &Read, 0);
            if(Success)
            {
                TotalBytesRead += Read;
                StdoutBufferP += Read;
                *StdoutBufferP = 0;
                BytesRemaining -= Read;
                
                if(BytesRemaining <= 0)
                {
                    ETWEndTrace(ETWEventTrace);
                    Quit("Aborting because the build output exceeded %5.1fmb.\n", StdoutBufferSize / 1024.0 / 1024.0);
                }
            }
            else
            {
                if(TotalBytesRead && (GetLastError() == ERROR_BROKEN_PIPE))
                {
                    GotFullStdout = true;
                }
                break;
            }
        }

        CloseHandle(ReadStdout);
        CloseHandle(ProcessInfo.hThread);
        ETWEndTrace(ETWEventTrace);

        if(!GotFullStdout)
        {
            Quit("Aborting because an unexpected error happened while reading the build output: %d\n", GetLastError());
        }

        DWORD ChildProcessExitCode;
        if(!GetExitCodeProcess(ProcessInfo.hProcess, &ChildProcessExitCode))
        {
            Quit("Unable to get the error code for the build script.  The error reported by the system is: %d\n", GetLastError());
        }

        CloseHandle(ProcessInfo.hProcess);

        if(ChildProcessExitCode)
        {
            Quit("The build script returned an error code: %d\n\n"
                 "BUILD OUTPUT (%.1fkb)\n"
                 "--------------------------\n%s\n",
                 ChildProcessExitCode,
                 TotalBytesRead / 1024.0,
                 StdoutBuffer);
        }
    }

    needed_file_list NeededFiles = {};
    NeededFiles.FileHead =
    NeededFiles.FileTail = PushStruct(needed_file);
    NeededFiles.Count = 0;

    // NOTE(chuck): Filter some items of interest
    // TODO(chuck): Merge this filtering block with the copy block below.  This filtering block used
    // to live inside an ETW callback that relied on globals.  After refactoring things out to
    // something a little more general-purpose that I might reuse elsewhere, there's really
    // no reason for two passes anymore.

    // Printf("# events: %d\n", ETWEventTrace->EventCount);
    etw_event *Event = ETWEventTrace->EventHead;
    while(Event)
    {
        switch(Event->Type)
        {
            case ETWType_FileIO:
            {
                // NOTE(chuck): Martins reminded me that cl.exe can launch many cl.exe child processes itself
                // with /MP.  Need to track all spawned processes so that they can all be filtered against.
                process *P = ProcessFilterHead;
                b32 Found = false;
                for(int Index = 0;
                    Index < ProcessFilterCount;
                    ++Index)
                {
                    if(Event->ProcessID == P->ProcessID)
                    {
                        Found = true;
                        break;
                    }
                    P = P->Next;
                }
                
                if(Found)
                {
                    wchar_t *DevicePath = Event->FileIO.Path;
                    wchar_t *Path = Win32DevicePathToDrivePath(&VolumeList, DevicePath);
                    // Log("Device path: %S\n  Drive path: %S\n", DevicePath, Path);

                    int PathLength = StringLength(Path);
                    b32 IsTelemetry =
                        // TODO(chuck): Do any of these have legit uses for reasons other than Telemetry?
                        // I guess it's also true that if you're literally working on something relating to
                        // Telemetry, they would be valid?  Turn all of this stuff into a user changeable
                        // whitelist/blacklist config file.
                        StringContains(Path, L"VCTIP.EXE") ||
                        StringContains(Path, L"Newtonsoft.Json.dll") ||
                        StringContains(Path, L"Microsoft.VisualStudio.Utilities.Internal.dll") ||
                        StringContains(Path, L"Microsoft.VisualStudio.Telemetry.dll") ||
                        StringContains(Path, L"Microsoft.VisualStudio.RemoteControl.dll");
                    b32 IsIgnore =
                        StringsAreEqualWithinLength(Path, FullBuildScript, FullBuildScriptLastSlashIndex) ||
                        IsTelemetry;
                    b32 IsWhitelistedDirectory =
                        StringContains(Path, L"\\Microsoft Visual Studio\\") ||
                        StringContains(Path, L"\\Windows Kits\\");
                    b32 IsWhitelistedExtension =
                        StringEndsWith(Path, L".exe") ||
                        StringEndsWith(Path, L".dll") ||
                        StringEndsWith(Path, L".lib") ||
                        StringEndsWith(Path, L".pdb") ||
                        StringEndsWith(Path, L".h");

                    b32 IsWanted = (IsWhitelistedDirectory && IsWhitelistedExtension);
                    if(!IsIgnore && IsWanted && FileExists(Path))
                    {
                        needed_file *NeededFile = NeededFiles.FileHead;
                        b32 IsFound = false;
                        int Count = NeededFiles.Count;
                        while(Count--)
                        {
                            if(StringsAreEqualWithinLength(NeededFile->Path, Path, PathLength + 1))
                            {
                                IsFound = true;
                                break;
                            }
                            NeededFile = NeededFile->Next;
                        }

                        // NOTE(chuck): Only add uniques                                            
                        if(!IsFound)
                        {
                            wchar_t *NewPathFilename = GetFilename(Path);
                            int NewPathFilenameLength = StringLength(NewPathFilename);
                            NeededFile = NeededFiles.FileHead;
                            Count = NeededFiles.Count;
                            while(Count--)
                            {
                                wchar_t *ExistingPathFilename = GetFilename(NeededFile->Path);
                                if(StringsAreEqualWithinLength(ExistingPathFilename, NewPathFilename, NewPathFilenameLength))
                                {
                                    Quit("Dependencies are not unique:\n  %S\n  %S\n", NeededFile->Path, Path);
                                }
                                NeededFile = NeededFile->Next;
                            }

                            NeededFile->Path = PushArray(PathLength + 1, wchar_t);
                            NeededFile->ProcessID = Event->ProcessID;
                            MemCopy(NeededFile->Path, Path, PathLength + 1);
                            NeededFile->Next =
                            NeededFiles.FileTail = PushStruct(needed_file);
                            ++NeededFiles.Count;
                        }
                    }
                }
            } break;

            case ETWType_Process:
            {
                // Printf("[process] %s %S\n", Event->Process.ImageFilename, Event->Process.CommandLine);
                process *P = ProcessFilterHead;
                b32 FoundChildProcess = false;
                for(int Index = 0;
                    Index < ProcessFilterCount;
                    ++Index)
                {
                    if(Event->Process.ParentProcessID == P->ProcessID)
                    {
                        FoundChildProcess = true;
                        break;
                    }
                    P = P->Next;
                }
                
                if(FoundChildProcess)
                {
                    process *Process = ProcessFilterTail;
                    Process->Next = PushStruct(process);
                    ProcessFilterTail = Process->Next;
                    Process->ImageFilename = Event->Process.ImageFilename;
                    Process->CommandLine = Event->Process.CommandLine;
                    Process->ProcessID = Event->ProcessID;
                    Process->ParentProcessID = Event->Process.ParentProcessID;
                    ++ProcessFilterCount;
                    // Log("Found another process: %d (Parent %d) %s %S\n",
                    //     Event->ProcessID, Event->Process.ParentProcessID,
                    //     Process->ImageFilename,
                    //     Process->CommandLine);
                    // Log("Total child processes: %d\n", ProcessFilterCount);
                }
            } break;
        }
        
        Event = Event->Next;
    }

    // NOTE(chuck): Transform the filtered stuff into usable paths. Rack up some tallies
    // TODO(chuck): Merge with the filtering block above.
    needed_file *NeededFile = NeededFiles.FileHead;
    b32 IsFound = false;
    int Count = NeededFiles.Count;
    char *TargetPathBuffer = PushSize(1024);
    wchar_t *CompilerDirectory = 0;
    while(Count--)
    {
        Log("%S\n", NeededFile->Path);
        char *TargetPath = TargetPathBuffer;
        wchar_t *TargetFilename = GetFilename(NeededFile->Path);
        if(StringEndsWith(NeededFile->Path, L".h"))
        {
            FormatString(1024, TargetPathBuffer, "%S\\%S", TargetIncludeDirectory, TargetFilename);
        }
        else if(StringEndsWith(NeededFile->Path, L".lib") ||
                StringEndsWith(NeededFile->Path, L".pdb"))
        {
            FormatString(1024, TargetPathBuffer, "%S\\%S", TargetLibDirectory, TargetFilename);
        }
        else
        {
            FormatString(1024, TargetPathBuffer, "%S\\%S", FullTargetDirectory, TargetFilename);

            if(StringsAreEqual(TargetFilename, L"cl.exe"))
            {
                if(!CompilerDirectory)
                {
                    int L = StringLength(TargetFilename) + 1;
                    CompilerDirectory = GetFullPath(NeededFile->Path);
                    smm Index = GetLastCharIndexInString(CompilerDirectory, '\\');
                    if(Index == -1)
                    {
                        Quit("Couldn't find the trailing backslash in the compiler directory: %S\n", TargetFilename);
                    }
                    CompilerDirectory[Index + 1] = 0;
                    // Log("Compiler directory %S\n", CompilerDirectory);
                }
            }
            else
            {
                // NOTE(chuck): Preserve cl.exe subdirectories (e.g. 1033 for American English locale)
                if(CompilerDirectory && StringStartsWith(NeededFile->Path, CompilerDirectory))
                {
                    wchar_t *RelativePath = NeededFile->Path + StringLength(CompilerDirectory);
                    wchar_t *P = RelativePath;
                    while(*P && *P != '\\') ++P;
                    if(*P == '\\')
                    {
                        // Printf("Preserving subdirectory for cl.exe dependency\n  %S\n", RelativePath);
                        TotalDirectoriesCreated += CreateDirectoriesRecursively(FullTargetDirectory, RelativePath);
                        FormatString(1024, TargetPathBuffer, "%S\\%S", FullTargetDirectory, RelativePath);
                    }
                }
            }
        }

        wchar_t *TargetPathW = WidenChars(TargetPath);
        if(!CopyFileW(NeededFile->Path, TargetPathW, 0))
        {
            Quit("Failed to copy file:\n  %S\n  %S\n  Error code %d\n", NeededFile->Path, TargetPathW, GetLastError());
        }

        if(StringEndsWith(NeededFile->Path, L".dll"))      ++TotalDLLFilesCopied;
        else if(StringEndsWith(NeededFile->Path, L".exe")) ++TotalEXEFilesCopied;
        else if(StringEndsWith(NeededFile->Path, L".h"))   ++TotalHFilesCopied;
        else if(StringEndsWith(NeededFile->Path, L".lib")) ++TotalLibFilesCopied;
        else if(StringEndsWith(NeededFile->Path, L".pdb")) ++TotalPDBFilesCopied;
        ++TotalFilesCopied;
        
        NeededFile = NeededFile->Next;
    }
    
    Log("\n");
    Log("%4d directories created\n", TotalDirectoriesCreated);
    Log("%4d .dll\n", TotalDLLFilesCopied);
    Log("%4d .exe\n", TotalEXEFilesCopied);
    Log("%4d .h\n", TotalHFilesCopied);
    Log("%4d .lib\n", TotalLibFilesCopied);
    Log("%4d .pdb\n", TotalPDBFilesCopied);
    Log("%4d total files copied\n", TotalFilesCopied);
    
    // Printf("%d bytes of memory used\n", GlobalMemoryUsed);
    // Printf("%d total files copied\n", TotalFilesCopied);
    Printf("Portable cl.exe generated in %S\n", FullTargetDirectory);
    // Printf("See %S for details.\n", LogPath.Data);

    return(0);
}
