static umm
PrintfList(DWORD Handle, char *Format, va_list Args)
{
    char Buffer[2048];
    umm Result = FormatStringList(2048, Buffer, Format, Args);

    HANDLE Out = GetStdHandle(Handle);
    DWORD CharsWritten;
    WriteConsoleA(Out, Buffer, (DWORD)Result, &CharsWritten, 0);

    return(Result);
}

static umm
Printf(DWORD Handle, char *Format, ...)
{
    va_list Args;

    va_start(Args, Format);
    umm Result = PrintfList(Handle, Format, Args);
    va_end(Args);

    return(Result);
}

static umm
Printf(char *Format, ...)
{
    va_list Args;

    va_start(Args, Format);
    umm Result = PrintfList(STD_OUTPUT_HANDLE, Format, Args);
    va_end(Args);

    return(Result);
}
