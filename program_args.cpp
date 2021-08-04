struct arg
{
    wchar_t *Value;
    umm Length;
    arg *Next;
};

struct program_args
{
    u32 Count;
    arg *Arg;
};

// NOTE(chuck): I'm mimicking CommandLineToArgvW as an exercise.  That is the
// reason for all the wonky parsing logic below.
program_args
GetProgramArgs(wchar_t *CommandLine)
{
    #define IsWhite(C) ((*(C) == ' ') || (*(C) == '\t'))

    program_args Result = {0, PushStruct(arg)};
    arg *Arg = Result.Arg;
    // NOTE(chuck): Just write directly into global memory and push the used amount
    // forward as necessary.
    Arg->Value = MemCurrent(wchar_t);

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
        MemSkip(Arg->Length, wchar_t); \
        ++Result.Count
    CommitArg;

    while(IsWhite(From)) ++From;
    if(*From)
    {
        #define NewArg \
            Arg->Next = PushStruct(arg); \
            Arg->Next->Value = To = MemCurrent(wchar_t); \
            Arg = Arg->Next
        NewArg;

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
                    NewArg;
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
    #undef NewArg
}
 
program_args
GetProgramArgs()
{
    wchar_t *CommandLine = GetCommandLineW();
    program_args Result = GetProgramArgs(CommandLine);
    return(Result);
}

wchar_t *
GetArg(program_args *ProgramArgs, u32 Index)
{
    arg *Arg = ProgramArgs->Arg;
    u32 N = 0;
    while(Arg && N < Index && N < ProgramArgs->Count)
    {
        ++N;
        Arg = Arg->Next;
    }
    return(Arg->Value);
}
