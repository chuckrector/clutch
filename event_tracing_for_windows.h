/*
NOTE(chuck): This is a very basic ETW wrapper which roughly follows the API described in
Casey Muratori's blog post titled "THE WORST API EVER MADE" https://caseymuratori.com/blog_0025
Many thanks to Mārtiņš Možeiko for sharing many code snippets and links to helpeful resources!
*/
#if !defined(EVENT_TRACING_FOR_WINDOWS_H)

struct Process_TypeGroup1
{
    uint64_t UniqueProcessKey;
    uint32_t ProcessId;
    uint32_t ParentId;
    uint32_t SessionId;
    int32_t ExitStatus;
    uint64_t DirectoryTableBase;
    uint32_t Flags;
    uint64_t Unknown1;
    uint32_t Unknown2;
    SID UserSID;
    // char ImageFilenameUTF8Z[];
    // wchar_t CommandLineUTF16Z[];
    // wchar_t PackageFullName[];
    // wchar_t ApplicationId[];    
};
typedef struct Process_TypeGroup1 Process_TypeGroup1;

// structures from "C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\km\wmicore.mof" (in Windows DDK)
struct FileIo_Create
{
    uint64_t IrpPtr;
    uint64_t FileObject;
    uint32_t TTID;
    uint32_t CreateOptions;
    uint32_t FileAttributes;
    uint32_t ShareAccess;
    wchar_t OpenPath[];
};
typedef struct FileIo_Create FileIo_Create;

struct process
{
    DWORD ProcessID;
    DWORD ParentProcessID;
    wchar_t *ImageFilename;
    wchar_t *CommandLine;
};
typedef struct process process;

enum etw_type
{
    ETWType_None,
    ETWType_ContextSwitch,
    ETWType_FileIO,
    ETWType_Process,

    ETWType_Count,
};
typedef enum etw_type etw_type;

struct etw_event_file_io
{
    wchar_t *Path;
    int PathLength;
};
typedef struct etw_event_file_io etw_event_file_io;

struct etw_event_process
{
    u32 ParentProcessID;
    wchar_t *ImageFilename;
    int ImageFilenameLength;
    wchar_t *CommandLine;
    int CommandLineLength;
};
typedef struct etw_event_process etw_event_process;

struct etw_event
{
    DWORD ProcessID;
    etw_type Type;
    union
    {
        etw_event_file_io FileIO;
        etw_event_process Process;
    };
};
typedef struct etw_event etw_event;

struct etw_internal_wnode
{
    EVENT_TRACE_PROPERTIES Properties;
    WCHAR SessionName[1024];
};
typedef struct etw_internal_wnode etw_internal_wnode;

struct etw_internal
{
    etw_internal_wnode *Wnode;
    TRACEHANDLE TraceHandle;
    HANDLE TraceThread;
};
typedef struct etw_internal etw_internal;

#define ETW_EVENT_MAX (1024 * 10)
struct etw_event_trace
{
    u64 Types; // NOTE(chuck): ETWAddEventType sums a bitmask of etw_types
    etw_event *EventList;
    int EventCount;

    int Error;
    void *Internal;
};
typedef struct etw_event_trace etw_event_trace;

// https://docs.microsoft.com/en-us/windows/win32/etw/nt-kernel-logger-constants
// http://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/etw/callouts/hookid.htm
static const GUID FileIoGuid   = {0x90cbdc39, 0x4a3e, 0x11d1, {0x84, 0xf4, 0x00, 0x00, 0xf8, 0x04, 0x64, 0xe3}};
static const GUID PerfInfoGuid = {0xce1dbfb4, 0x137e, 0x4da6, {0x87, 0xb0, 0x3f, 0x59, 0xaa, 0x10, 0x2c, 0xbc}};
static const GUID ProcessGuid  = {0x3d6fa8d0, 0xfe05, 0x11d0, {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c}};

static void WINAPI TraceEventRecordCallback(EVENT_RECORD *Event);
static DWORD WINAPI TraceProcessThread(void *ThreadParameter);
static etw_event_trace *ETWBeginTrace();
static void ETWEndTrace(etw_event_trace *ETWEventTrace);
static void ETWAddEventType(etw_event_trace *Trace, etw_type Type);

#define EVENT_TRACING_FOR_WINDOWS_H
#endif
