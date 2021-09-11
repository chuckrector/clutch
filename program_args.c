#define PROGRAM_ARG_MAX (1024 * 4)
struct arg
{
    wchar_t *Value;
    umm Length;
};
typedef struct arg arg;

struct program_args
{
    u32 Count;
    arg *ArgList;
};
typedef struct program_args program_args;

static arg *
AddProgramArg(program_args *ProgramArgs)
{
    if(ProgramArgs->Count >= PROGRAM_ARG_MAX)
    {
        Quit("Too many program args.  Maximum allowed: %d\n", PROGRAM_ARG_MAX);
    }

    arg *Result = ProgramArgs->ArgList + ProgramArgs->Count++;
    // NOTE(chuck): Just write directly into global memory and push the used amount
    // forward as necessary.
    Result->Value = MemCurrent(wchar_t);

    return(Result);
}

// NOTE(chuck): I'm mimicking CommandLineToArgvW as an exercise.  That is the
// reason for all the wonky parsing logic below.
static program_args
GetProgramArgs(wchar_t *CommandLine)
{
    #define IsWhite(C) ((*(C) == ' ') || (*(C) == '\t'))

    program_args Result = {0, PushArray(PROGRAM_ARG_MAX, arg)};
    arg *Arg = AddProgramArg(&Result);

    wchar_t *From = CommandLine;
    wchar_t *To = Arg->Value;
    // NOTE(chuck): The .exe is treated specially and no escaping is performed.
    // For a quoted .exe, parse to the next quote. Otherwise, parse to the next scape.
    if(*From == '"')
    {
        ++From;
        while(*From && *From != '"') *To++ = *From++;
        if(*From == '"') ++From;
    }
    else
    {
        while(*From && !IsWhite(From)) *To++ = *From++;
        if(*From) ++From;
    }

    #define CommitArg \
        *To++ = 0; \
        Arg->Length = To - Arg->Value; \
        MemSkip(Arg->Length, wchar_t);
    CommitArg;

    while(IsWhite(From)) ++From;
    if(*From)
    {
        Arg = AddProgramArg(&Result);
        To = Arg->Value;

        int Quotes = 0;
        int Slashes = 0;
        while(*From)
        {
            if(IsWhite(From) && !Quotes)
            {
                Slashes = 0;
                while(IsWhite(From)) ++From;
                if(*From)
                {
                    CommitArg;
                    Arg = AddProgramArg(&Result);
                }
            }
            else if(*From == '\\')
            {
                *To++ = *From++;
                ++Slashes;
            }
            else if(*From == '"')
            {
                // NOTE(chuck): This is the terrifying logic for resolving escapes.
                // Perhaps unsurprisingly, it yields different results than the usual
                // main(int argc, char **argv) variety in certain cases.
                //
                // For example:
                //
                //     a"b"" c d
                //
                // yields four arguments from CommandLineToArgvW.  main() yields one.
                //
                if(Slashes % 2) To -= ((Slashes / 2) + 1), *To++ = '"';
                else To -= (Slashes / 2), ++Quotes;
                ++From;
                Slashes = 0;
                while(*From == '"')
                {
                    if(++Quotes == 3) *To++ = '"', Quotes = 0;
                    ++From;
                }
                if(Quotes == 2) Quotes = 0;
            }
            else
            {
                *To++ = *From++;
                Slashes = 0;
            }
        }

        CommitArg;
    }

    return(Result);

    #undef IsWhite
    #undef CommitArg
}
 
static program_args
GetProgramArgsAuto()
{
    wchar_t *CommandLine = GetCommandLineW();
    program_args Result = GetProgramArgs(CommandLine);
    return(Result);
}

static wchar_t *
GetArg(program_args *ProgramArgs, u32 Index)
{
    Assert(ProgramArgs);
    if((Index < 0) || (Index >= ProgramArgs->Count))
    {
        Quit("Program arg index out of range: %d\n  Total program args available: %d\n", Index, ProgramArgs->Count);
    }

    arg *Arg = ProgramArgs->ArgList + Index;
    wchar_t *Result = Arg->Value;
    return(Result);
}
