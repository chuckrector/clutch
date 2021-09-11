#define INITGUID
#define NOMINMAX

#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <stdint.h>
#include <stdio.h>

#include "literals.h"
#include "types.h"
#include "memory.h"
#include "strings.h"
#include "files.h"
#include "debug.h"
#include "event_tracing_for_windows.h"

#include "print.c"
#include "debug.c"
#include "memory.c"
#include "strings.c"
#include "program_args.c"
#include "files.c"
#include "event_tracing_for_windows.c"

struct needed_file
{
    wchar_t *Path;
    DWORD ProcessID;
};
typedef struct needed_file needed_file;
