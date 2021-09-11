/*

:"program.exe" "a b c" d e
4
program.exe
a b c
d
e

:"program.exe" "ab\"c" "\\" d
4
program.exe
ab"c
\
d

:"program.exe" a\\\b d"e f"g h
4
program.exe
a\\\b
de fg
h

:"program.exe" a\\\"b c d
4
program.exe
a\"b
c
d

:"program.exe" a\\\\"b c" d e
4
program.exe
a\\b c
d
e

:"program.exe" a"b""a c d
4
program.exe
ab"a
c
d

*/

#include <windows.h>
#include <stdio.h>

#include "types.h"
#include "strings.h"
#include "debug.h"
#include "strings.h"
#include "files.h"

#include "print.c"
#include "debug.c"
#include "memory.c"
#include "strings.c"
#include "program_args.c"
#include "files.c"

int
main()
{
    int Result = 0;
    wchar_t *FileW = WidenChars(__FILE__);
    HANDLE Handle = CreateFileW(FileW, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if(Handle)
    {
        DWORD BytesRead;
        if(ReadFile(Handle, GlobalMemory, sizeof(GlobalMemory), &BytesRead, 0))
        {
            GlobalMemory[BytesRead] = 0;
            MemSkip(BytesRead + 1, char);

            char *P = GlobalMemory;
            char *End = P;
            while(*End && !IsEndComment(End)) ++End;

            P = GlobalMemory;
            while(P < End)
            {
                // NOTE(chuck): ":" begins each test
                while(P < End && *P != ':') ++P;
                if(P < End)
                {
                    ++P;
                    char *TestInput = P;
                    while(P < End && *P != '\n') ++P;
                    char *TestInputEnd = P;
                    *P++ = 0;

                    u32 ExpectedArgCount;
                    sscanf(P, "%d", &ExpectedArgCount);
                    while(P < End && *P != '\n') ++P;
                    ++P;

                    int TestInputLength = (int)(TestInputEnd - TestInput);
                    wchar_t *TestInputW = WidenChars(TestInput);
                    Printf("Testing %S\n", TestInputW);

                    program_args ProgramArgs = GetProgramArgs(TestInputW);
                    if(ProgramArgs.Count == ExpectedArgCount)
                    {
                        for(u32 Index = 0;
                            Index < ProgramArgs.Count;
                            ++Index)
                        {
                            char *ExpectedArg = P;
                            while(P < End && *P != '\n') ++P;
                            int ExpectedArgLength = (int)(P - ExpectedArg);
                            *P++ = 0;

                            wchar_t *ExpectedArgW = WidenChars(ExpectedArg);
                            wchar_t *Arg = GetArg(&ProgramArgs, Index);
                            if(!StringsAreEqualWW(ExpectedArgW, Arg))
                            {
                                PrintfH(STD_ERROR_HANDLE, "FAIL: Expected argument #%d to be `%S` but got `%S`\n", Index, ExpectedArgW, Arg);
                                Result = 1;
                            }
                        }
                    }
                    else
                    {
                        fprintf(stderr, "FAIL: Expected %d args but got %d\n", ExpectedArgCount, ProgramArgs.Count);
                        for(u32 Index = 0;
                            Index < ProgramArgs.Count;
                            ++Index)
                        {
                            fprintf(stderr, "Args[%d] %S\n", Index, GetArg(&ProgramArgs, Index));
                        }
                        Result = 1;
                    }
                }
            }
        }
        else
        {
            PrintfH(STD_ERROR_HANDLE, "Couldn't read file.  Error code %d\n", GetLastError());
            Result = 1;
        }

        CloseHandle(Handle);
    }

    if(!Result)
    {
        Printf("PASS\n");
    }

    return(Result);
}