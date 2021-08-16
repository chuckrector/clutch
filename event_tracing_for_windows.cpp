// NOTE(chuck): Massage M$ crapola into stuff I care about.
static void WINAPI
TraceEventRecordCallback(EVENT_RECORD *Event)
{
    GUID *Provider = &Event->EventHeader.ProviderId;
    UCHAR OpCode = Event->EventHeader.EventDescriptor.Opcode;

    // NOTE(chuck): EVENT_TRACE_LOGFILEW.Context was set to an etw_event_trace
    etw_event_trace *ETWEventTrace = (etw_event_trace *)Event->UserContext;

    #define AddEvent \
        etw_event *ETWEvent = ETWEventTrace->EventTail; \
        ETWEvent->Next = PushStruct(etw_event); \
        ETWEventTrace->EventTail = ETWEvent->Next; \
        ++ETWEventTrace->EventCount;

    if((ETWEventTrace->Types & (1LL << ETWType_FileIO)) &&
       IsEqualGUID(*Provider, FileIoGuid))
    {
        if(OpCode == 0x40) // PERFINFO_LOG_TYPE_FILE_IO_CREATE
        {
            AddEvent;

            FileIo_Create *Data = (FileIo_Create *)Event->UserData;

            wchar_t *Path = Data->OpenPath;
            int PathLength = StringLength(Path);

            // Log("ETW (PID %d) %S\n", Event->EventHeader.ProcessId, Path);

            ETWEvent->Type = ETWType_FileIO;
            ETWEvent->ProcessID = Event->EventHeader.ProcessId;
            ETWEvent->FileIO.Path = PushArray(PathLength + 1, wchar_t);
            ETWEvent->FileIO.PathLength = PathLength;
            MemCopy(ETWEvent->FileIO.Path, Path, PathLength + 1);
        }
    }
    else if((ETWEventTrace->Types & (1LL << ETWType_Process)) &&
            IsEqualGUID(*Provider, ProcessGuid))
    {
        if(OpCode == EVENT_TRACE_TYPE_START)
        {
            AddEvent;

            Process_TypeGroup1 *Data = (Process_TypeGroup1 *)Event->UserData;
            Assert(IsValidSid(&Data->UserSID));
            DWORD SIDLength = GetLengthSid(&Data->UserSID);

            char *ImageFilename = (char *)&Data->UserSID + SIDLength;
            int ImageFilenameLength = StringLength(ImageFilename);
            wchar_t *CommandLine = (wchar_t *)(ImageFilename + ImageFilenameLength + 1);
            int CommandLineLength = StringLength(CommandLine);

            ETWEvent->Type = ETWType_Process;
            ETWEvent->ProcessID = Data->ProcessId;
            ETWEvent->Process.ParentProcessID = Data->ParentId;

            // NOTE(chuck): WidenChars allocates and copies, so no need to copy here.
            ETWEvent->Process.ImageFilename = WidenChars(ImageFilename);
            ETWEvent->Process.ImageFilenameLength = ImageFilenameLength;

            // NOTE(chuck): We only allocate here, so a copy is needed.
            ETWEvent->Process.CommandLine = PushArray(CommandLineLength + 1, wchar_t);
            ETWEvent->Process.CommandLineLength = CommandLineLength;
            CopyString(ETWEvent->Process.CommandLine, CommandLine);
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
    Result->EventHead = PushStruct(etw_event);
    Result->EventTail = Result->EventHead;
    Result->Internal = Internal;
    Result->Error = 666;

    HANDLE Token;
    if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY|TOKEN_ADJUST_PRIVILEGES, &Token))
    {
        Quit("Unable to open process token.  Error code %d\n", GetLastError());
    }

    TOKEN_ELEVATION Elevation;
    DWORD Size;
    if(!GetTokenInformation(Token, TokenElevation, &Elevation, sizeof(Elevation), &Size))
    {
        Quit("Unable get token information.  Error code %d\n", GetLastError());
    }
        
    if(!Elevation.TokenIsElevated)
    {
        Quit("Elevated privileges are required.  Error code %d\n", GetLastError());
    }

    LUID LocallyUniqueID;
    if(!LookupPrivilegeValueA(0, SE_SYSTEM_PROFILE_NAME, &LocallyUniqueID))
    {
        Quit("Unable to lookup profiling privilege.  Error code %d\n", GetLastError());
    }

    TOKEN_PRIVILEGES TokenPrivileges;
    TokenPrivileges.PrivilegeCount = 1;
    TokenPrivileges.Privileges[0].Luid = LocallyUniqueID;
    TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    if(!AdjustTokenPrivileges(Token, 0, &TokenPrivileges, sizeof(TokenPrivileges), 0, 0))
    {
        Quit("Unable to adjust token privilege.  Error code %d\n", GetLastError());
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
        Quit("Unable to start trace.  Error code %d\n", StartTraceResult);
    }

    EVENT_TRACE_LOGFILEW TraceLogFile = {};
    TraceLogFile.LoggerName = KERNEL_LOGGER_NAMEW;
    TraceLogFile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD|PROCESS_TRACE_MODE_RAW_TIMESTAMP|PROCESS_TRACE_MODE_REAL_TIME;
    TraceLogFile.EventRecordCallback = TraceEventRecordCallback;
    TraceLogFile.Context = Result;

    Internal->TraceHandle = OpenTraceW(&TraceLogFile);
    if(Internal->TraceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        ControlTraceResult = ControlTraceW(0, KERNEL_LOGGER_NAMEW, TraceProps, EVENT_TRACE_CONTROL_STOP);

        Quit("Unable to open trace.  Exit code %d\n", GetLastError());
    }

    Internal->TraceThread = CreateThread(0, 0, TraceProcessThread, &Internal->TraceHandle, 0, 0);
    if(!Internal->TraceThread)
    {
        ControlTraceW(0, KERNEL_LOGGER_NAMEW, TraceProps, EVENT_TRACE_CONTROL_STOP);
        CloseTrace(Internal->TraceHandle);

        Quit("Unable to create trace thread.  Error code %d\n", GetLastError());
    }

    Result->Error = 0;

    CloseHandle(Token);

    // printf("Trace began.\n");
    return(Result);
}

static void
ETWEndTrace(etw_event_trace *ETWEventTrace)
{
    etw_internal *Internal = (etw_internal *)ETWEventTrace->Internal;
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
