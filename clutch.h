#define INITGUID
#define NOMINMAX

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <stdint.h>

#include "types.h"
#include "strings.h"
#include "files.h"
#include "debug.h"
#include "event_tracing_for_windows.h"

#include "print.cpp"
#include "debug.cpp"
#include "memory.cpp"
#include "strings.cpp"
#include "program_args.cpp"
#include "files.cpp"
#include "event_tracing_for_windows.cpp"

struct needed_file
{
    wchar_t *Path;
    DWORD ProcessID;
};
