static etw_event *
ETWEventAdd(etw_event_trace *Trace)
{
    if(Trace->EventCount >= ETW_EVENT_MAX)
    {
        Quit("Too many file I/O events occurred -- over %d.\n%s",
             ETW_EVENT_MAX, CHANGE_REQUIRES_RECOMPILE);
    }
    etw_event *Result = Trace->EventList + Trace->EventCount++;
    return(Result);
}

// NOTE(chuck): Massage M$ crapola into stuff I care about.
static void WINAPI
TraceEventRecordCallback(EVENT_RECORD *EventRecord)
{
    GUID *Provider = &EventRecord->EventHeader.ProviderId;
    UCHAR OpCode = EventRecord->EventHeader.EventDescriptor.Opcode;

    // NOTE(chuck): EVENT_TRACE_LOGFILEW.Context was set to an etw_event_trace
    etw_event_trace *Trace = (etw_event_trace *)EventRecord->UserContext;

    if((Trace->Types & (1LL << ETWType_FileIO)) &&
       IsEqualGUID(Provider, &FileIoGuid)) // NOTE(chuck): Interesting how the signature changes for C!
    {
        if(OpCode == 0x40) // PERFINFO_LOG_TYPE_FILE_IO_CREATE
        {
            etw_event *ETWEvent = ETWEventAdd(Trace);

            FileIo_Create *Data = (FileIo_Create *)EventRecord->UserData;

            wchar_t *Path = Data->OpenPath;
            int PathLength = StringLengthW(Path);

            // Log("ETW (PID %d) %S\n", Event->EventHeader.ProcessId, Path);

            ETWEvent->Type = ETWType_FileIO;
            ETWEvent->ProcessID = EventRecord->EventHeader.ProcessId;
            ETWEvent->FileIO.Path = PushArray(PathLength + 1, wchar_t);
            ETWEvent->FileIO.PathLength = PathLength;
            MemCopyWW(ETWEvent->FileIO.Path, Path, PathLength + 1);
        }
    }
    else if((Trace->Types & (1LL << ETWType_Process)) &&
            IsEqualGUID(Provider, &ProcessGuid)) // NOTE(chuck): Interesting how the signature changes for C!
    {
        if(OpCode == EVENT_TRACE_TYPE_START)
        {
            etw_event *ETWEvent = ETWEventAdd(Trace);

            Process_TypeGroup1 *Data = (Process_TypeGroup1 *)EventRecord->UserData;
            Assert(IsValidSid(&Data->UserSID));
            DWORD SIDLength = GetLengthSid(&Data->UserSID);

            char *ImageFilename = (char *)&Data->UserSID + SIDLength;
            int ImageFilenameLength = StringLengthC(ImageFilename);
            wchar_t *CommandLine = (wchar_t *)(ImageFilename + ImageFilenameLength + 1);
            int CommandLineLength = StringLengthW(CommandLine);

            ETWEvent->Type = ETWType_Process;
            ETWEvent->ProcessID = Data->ProcessId;
            ETWEvent->Process.ParentProcessID = Data->ParentId;

            // NOTE(chuck): WidenChars allocates and copies, so no need to copy here.
            ETWEvent->Process.ImageFilename = WidenChars(ImageFilename);
            ETWEvent->Process.ImageFilenameLength = ImageFilenameLength;

            // NOTE(chuck): We only allocate here, so a copy is needed.
            ETWEvent->Process.CommandLine = PushArray(CommandLineLength + 1, wchar_t);
            ETWEvent->Process.CommandLineLength = CommandLineLength;
            CopyStringWW(ETWEvent->Process.CommandLine, CommandLine);
        }
    }

    #undef AddEvent
}

static DWORD WINAPI
TraceProcessThread(void *ThreadParameter)
{
    TRACEHANDLE *TraceHandle = (TRACEHANDLE *)ThreadParameter;
    ProcessTrace(TraceHandle, 1, 0, 0);
    return(0);
}

static etw_event_trace *
ETWBeginTrace()
{
    etw_event_trace *Result = PushStruct(etw_event_trace);
    etw_internal *Internal = PushStruct(etw_internal);
    Internal->Wnode = PushStruct(etw_internal_wnode);
    Result->EventList = PushArray(ETW_EVENT_MAX, etw_event);
    Result->Internal = Internal;
    Result->Error = 666;

    HANDLE Token;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES, &Token))
    {
        Quit("Event Tracing for Windows could not begin because an access token could not be obtained.\n"
             "The access token is needed in order to determine whether elevated privileges are available.\n");
    }

    TOKEN_ELEVATION Elevation;
    DWORD Size;
    if(!GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &Size))
    {
        Quit("Event Tracing for Windows could not begin because there was a problem querying the access token.\n"
             "Elevated privileges are required in order to run this tool and it cannot be determined whether\n"
             "those privilieges are available unless the access token can be queried.\n");
    }
        
    if(!Elevation.TokenIsElevated)
    {
        Quit("Event Tracing for Windows could not begin because elevated privileges are required.\n"
             "Please relaunch Clutch from a command-line that has administrator privileges.\n");
    }

    LUID LocallyUniqueID;
    if(!LookupPrivilegeValueA(0, SE_SYSTEM_PROFILE_NAME, &LocallyUniqueID))
    {
        Quit("Event Tracing for Windows could not begin because there was a problem looking up the\n"
             "LUID (locally unique identifier) for system profiling privileges.\n");
    }

    TOKEN_PRIVILEGES TokenPrivileges;
    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0].Luid = LocallyUniqueID;
    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    if(!AdjustTokenPrivileges(Token, 0, &TokenPrivileges, sizeof(TokenPrivileges), 0, 0))
    {
        Quit("Event Tracing for Windows could not begin because there was a problem enabling system profiling privileges.\n");
    }

    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    
    EVENT_TRACE_PROPERTIES *TraceProps = &Internal->Wnode->Properties;
    MemClear(TraceProps, sizeof(*TraceProps));
    TraceProps->Wnode.BufferSize = sizeof(*Internal->Wnode);
    TraceProps->Wnode.Guid = SystemTraceControlGuid;
    TraceProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    TraceProps->LoggerNameOffset = sizeof(Internal->Wnode->Properties);
    ULONG ControlTraceResult = ControlTraceW(0, KERNEL_LOGGER_NAMEW, TraceProps, EVENT_TRACE_CONTROL_STOP);
    
    MemClear(TraceProps, sizeof(*TraceProps));
    TraceProps->Wnode.BufferSize = sizeof(*Internal->Wnode);
    TraceProps->Wnode.Guid = SystemTraceControlGuid;
    TraceProps->Wnode.ClientContext = 1;
    TraceProps->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    TraceProps->BufferSize = 1024; // 1MiB
    TraceProps->MinimumBuffers = 2 * SystemInfo.dwNumberOfProcessors;
    TraceProps->MaximumBuffers = TraceProps->MinimumBuffers + 20;
    TraceProps->LogFileMode = EVENT_TRACE_REAL_TIME_MODE|EVENT_TRACE_SYSTEM_LOGGER_MODE;
    TraceProps->LoggerNameOffset = sizeof(Internal->Wnode->Properties);
    TraceProps->FlushTimer = 1;
    TraceProps->EnableFlags = EVENT_TRACE_FLAG_FILE_IO_INIT|EVENT_TRACE_FLAG_PROCESS;
    
    TRACEHANDLE Session;
    ULONG StartTraceResult = StartTraceW(&Session, KERNEL_LOGGER_NAMEW, TraceProps);
    if(StartTraceResult != ERROR_SUCCESS)
    {
        Quit("Event Tracing for Windows could not begin because starting the trace failed.\n");
    }

    EVENT_TRACE_LOGFILEW TraceLogFile = {0};
    TraceLogFile.LoggerName = KERNEL_LOGGER_NAMEW;
    TraceLogFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD|PROCESS_TRACE_MODE_RAW_TIMESTAMP|PROCESS_TRACE_MODE_REAL_TIME;
    TraceLogFile.EventRecordCallback = TraceEventRecordCallback;
    TraceLogFile.Context = Result;

    Internal->TraceHandle = OpenTraceW(&TraceLogFile);
    if(Internal->TraceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        ControlTraceResult = ControlTraceW(0, KERNEL_LOGGER_NAMEW, TraceProps, EVENT_TRACE_CONTROL_STOP);

        Quit("Event Tracing for Windows could not begin because opening the trace failed.\n", GetLastError());
    }

    Internal->TraceThread = CreateThread(0, 0, TraceProcessThread, &Internal->TraceHandle, 0, 0);
    if(!Internal->TraceThread)
    {
        ControlTraceW(0, KERNEL_LOGGER_NAMEW, TraceProps, EVENT_TRACE_CONTROL_STOP);
        CloseTrace(Internal->TraceHandle);

        Quit("Event Tracing for Windows could not begin because a new thread for processing "
             "trace event records could not be created.\n");
    }

    Result->Error = 0;

    CloseHandle(Token);

    // printf("Trace began.\n");
    return(Result);
}

static void
ETWEndTrace(etw_event_trace *Trace)
{
    etw_internal *Internal = (etw_internal *)Trace->Internal;
    ULONG StopTraceResult = ControlTraceW(0, KERNEL_LOGGER_NAMEW, &Internal->Wnode->Properties, EVENT_TRACE_CONTROL_STOP);
    CloseTrace(Internal->TraceHandle);
    WaitForSingleObject(Internal->TraceThread, INFINITE);

    // printf("Trace ended.\n");
}

static void
ETWAddEventType(etw_event_trace *Trace, etw_type Type)
{
    Trace->Types |= (1LL << Type);
}
