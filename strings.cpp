// #define STD_INPUT_HANDLE  ((u32)-10)
// #define STD_OUTPUT_HANDLE ((u32)-11)
// #define STD_ERROR_HANDLE  ((u32)-12)

static int
StringLength(char *String)
{
    int Result = 0;
    while(*String)
    {
        ++String;
        ++Result;
    }
    return(Result);
}

static int
StringLength(wchar_t *String)
{
    int Result = 0;
    while(*String)
    {
        ++String;
        ++Result;
    }
    return(Result);
}

static void
CopyString(char *To, char *From)
{
    while(*From) *To++ = *From++;
}

static void
CopyString(wchar_t *To, wchar_t *From)
{
    while(*From) *To++ = *From++;
}

static b32
IsCRLF(char *P)
{
    b32 Result = (P[0] == '\r') && (P[1] == '\n');
    return(Result);
}

static b32
IsLF(char *P)
{
    b32 Result = P[0] == '\n';
    return(Result);
}

static b32
IsCharEqual(wchar_t A, wchar_t B)
{
    wchar_t UpperA = A;
    wchar_t UpperB = B;
    if(A >= 'a' && A <= 'z') UpperA -= 32;
    if(B >= 'a' && B <= 'z') UpperB -= 32;
    b32 Result = (UpperA == UpperB);
    return(Result);
}

static b32
StringEndsWith(wchar_t *String, wchar_t *Suffix)
{
    wchar_t *StringIter = String + StringLength(String);
    wchar_t *SuffixIter = Suffix + StringLength(Suffix);
    while(SuffixIter > Suffix &&
          StringIter > String &&
          IsCharEqual(SuffixIter[-1], StringIter[-1]))
    {
        --SuffixIter;
        --StringIter;
    }
    b32 Result = (Suffix == SuffixIter);
    return(Result);
}

static b32
StringsAreEqual(char *A, char *B)
{
    while(*A && *B && IsCharEqual(*A, *B))
    {
        ++A;
        ++B;
    }
    b32 Result = !*A && !*B;
    return(Result);
}

static b32
StringsAreEqual(wchar_t *A, wchar_t *B)
{
    while(*A && *B && IsCharEqual(*A, *B))
    {
        ++A;
        ++B;
    }
    b32 Result = !*A && !*B;
    return(Result);
}

static b32
StringStartsWith(wchar_t *String, wchar_t *Prefix)
{
    while(*String && *Prefix && IsCharEqual(*String, *Prefix))
    {
        ++String;
        ++Prefix;
    }
    b32 Result = !*Prefix;
    return(Result);
}

static b32
StringsAreEqualWithinLength(wchar_t *A, wchar_t *B, umm Size)
{
    while(IsCharEqual(*A, *B) && Size) 
    {
        ++A;
        ++B;
        --Size;
    }
    b32 Result = !Size;
    return(Result);
}

static b32
StringContains(wchar_t *String, wchar_t *Substring)
{
    int L = StringLength(String);
    int SubstringLength = StringLength(Substring);
    b32 Result = false;
    if(SubstringLength <= L)
    {
        wchar_t *C = String;
        wchar_t *ComparisonEnd = String + L - SubstringLength;
        while(*C && C <= ComparisonEnd)
        {
            if(StringsAreEqualWithinLength(C, Substring, SubstringLength))
            {
                Result = true;
                break;
            }
            ++C;
        }
    }
    return(Result);
}

static b32
IsEndComment(char *P)
{
    b32 Result = ((P[0] == '*') && (P[1] == '/'));
    return(Result);
}

static wchar_t *
WidenChars(char *String)
{
    int Length = StringLength(String);
    wchar_t *Result = PushArray(Length + 1, wchar_t);
    MultiByteToWideChar(CP_UTF8, 0, String, Length, Result, Length);
    return(Result);
}

static char *
UnwidenChars(wchar_t *String)
{
    int Length = StringLength(String);
    char *Result = PushArray(Length + 1, char);
    WideCharToMultiByte(CP_UTF8, 0, String, Length, Result, Length, 0, 0);
    return(Result);
}

static void
AppendChar(wide_buffer *Buffer, wchar_t Char)
{
    if(Buffer->Count < Buffer->MaxCount) Buffer->Data[Buffer->Count++] = Char;
}

static void
AppendString(wide_buffer *Buffer, wchar_t *String)
{
    while(*String) AppendChar(Buffer, *String++);
}

static int
AppendDecimal(wide_buffer *Buffer, u32 Value)
{
    int CharsWritten = 0;
    u32 Remains = Value;
    for(int Divisor = 1000000000;
        Divisor > 0;
        Divisor /= 10)
    {
        int Digit = Remains / Divisor;
        Remains -= Digit*Divisor;

        if(Digit || (Value != Remains) || (Divisor == 1))
        {
            AppendChar(Buffer, (char)('0' + Digit));
            ++CharsWritten;
        }
    }
    return(CharsWritten);
}

static void
AppendDouble(wide_buffer *Buffer, double Value, u32 Precision = 2)
{
    if(Value < 0)
    {
        AppendChar(Buffer, '-');
        Value = -Value;
    }

    u32 IntegerPart = (u32)Value;
    Value -= (f32)IntegerPart;
    AppendDecimal(Buffer, IntegerPart);
    AppendChar(Buffer, '.');

    for(u32 PrecisionIndex = 0;
        PrecisionIndex < Precision;
        ++PrecisionIndex)
    {
        Value *= 10.0f;
        u32 Integer = (u32)Value;
        Value -= (f32)Integer;
        AppendChar(Buffer, DecChars[Integer]);
    }
}

static void
OutChar(format_dest *Dest, char Value)
{
    if(Dest->Size)
    {
        --Dest->Size;
        *Dest->At++ = Value;
    }
}

static void
OutChars(format_dest *Dest, char *String)
{
    while(*String) OutChar(Dest, *String++);
}

static s32
S32FromZInternal(char **AtInit)
{
    s32 Result = 0;    

    char *At = *AtInit;
    while((*At >= '0') &&
          (*At <= '9'))
    {
        Result *= 10;
        Result += (*At - '0');
        ++At;
    }

    *AtInit = At;

    return(Result);
}

static s32
S32FromZ(char *At)
{
    char *Ignored = At;
    s32 Result = S32FromZInternal(&At);
    return(Result);
}

static u64
ReadVarArgSignedInteger(u32 Length, va_list *Args)
{
    u64 Result = 0;
    switch(Length)
    {
        case 1:
        {
            Result = va_arg(*Args, u8);
        } break;

        case 2:
        {
            Result = va_arg(*Args, u16);
        } break;

        case 4:
        {
            Result = va_arg(*Args, u32);
        } break;

        case 8:
        {
            Result = va_arg(*Args, u64);
        } break;
    }
    return(Result);
}

static u64
ReadVarArgUnsignedInteger(u32 Length, va_list *Args)
{
    u64 Temp = ReadVarArgSignedInteger(Length, Args);
    s64 Result = *(s64 *)&Temp;
    return(Result);
}

static f64
ReadVarArgFloat(u32 Length, va_list *Args)
{
    f64 Result = 0;
    switch(Length)
    {
        case 4:
        {
            Result = va_arg(*Args, f32);
        } break;

        case 8:
        {
            Result = va_arg(*Args, f64);
        } break;
    }
    return(Result);
}

static void
U64ToASCII(format_dest *Dest, u64 Value, u32 Base, char *Digits)
{
    Assert(Base);

    char *Start = Dest->At;
    do
    {
        u64 DigitIndex = (Value % Base);
        char Digit = Digits[DigitIndex];
        OutChar(Dest, Digit);

        Value /= Base;
    } while(Value);

    char *End = Dest->At;
    while(Start < End)
    {
        --End;
        char Temp = *End;
        *End = *Start;
        *Start = Temp;
        ++Start;
    }
}

static void
F64ToASCII(format_dest *Dest, f64 Value, u32 Precision)
{
    if(Value < 0)
    {
        OutChar(Dest, '-');
        Value = -Value;
    }

    u64 IntegerPart = (u64)Value;
    Value -= (f64)IntegerPart;
    U64ToASCII(Dest, IntegerPart, 10, DecChars);

    OutChar(Dest, '.');

    for(u32 PrecisionIndex = 0;
        PrecisionIndex < Precision;
        ++PrecisionIndex)
    {
        Value *= 10.0f;
        u32 Integer = (u32)Value;
        Value -= (f32)Integer;
        OutChar(Dest, DecChars[Integer]);
    }
}

static umm
FormatStringList(umm DestSize, char *DestInit, char *Format, va_list Args)
{
    format_dest Dest = {DestSize, DestInit};
    if(Dest.Size)
    {
        char *At = Format;
        while(At[0])
        {
            if(*At == '%')
            {
                ++At;

                b32 ForceSign = false;
                b32 PadWithZeros = false;
                b32 LeftJustify = false;
                b32 PositiveSignIsBlank = false;
                b32 AnnotateIfNotZero = false;

                b32 Parsing = true;
                while(Parsing)
                {
                    switch(*At)
                    {
                        case '+': {ForceSign = true;} break;
                        case '0': {PadWithZeros = true;} break;
                        case '-': {LeftJustify = true;} break;
                        case ' ': {PositiveSignIsBlank = true;} break;
                        case '#': {AnnotateIfNotZero = true;} break;
                        default: {Parsing = false;} break;
                    }

                    if(Parsing)
                    {
                        ++At;
                    }
                }

                b32 WidthSpecified = false;
                s32 Width = 0;
                if(*At == '*')
                {
                    Width = va_arg(Args, int);
                    WidthSpecified = true;
                    ++At;
                }
                else if((*At >= '0') && (*At <= '9'))
                {
                    Width = S32FromZInternal(&At);
                    WidthSpecified = true;
                    // ++At; // NOTE(chuck): S32FromZInternal increments At
                }

                // NOTE(chuck): Precision
                b32 PrecisionSpecified = false;
                s32 Precision = 0;
                if(*At == '.')
                {
                    ++At;

                    if(*At == '*')
                    {
                        Precision = va_arg(Args, int);
                        PrecisionSpecified = true;
                        ++At;
                    }
                    else if((*At >= '0') && (*At <= '9'))
                    {
                        Precision = S32FromZInternal(&At);
                        PrecisionSpecified = true;
                        // ++At; // NOTE(chuck): S32FromZInternal increments At
                    }
                    else
                    {
                        Assert(!"Malformed precision specifier!");
                    }
                }

                if(!PrecisionSpecified)
                {
                    Precision = 6;
                }
                // NOTE(chuck): Length
                u32 IntegerLength = 4;
                u32 FloatLength = 8;
                if((At[0] == 'h') && (At[1] == 'h'))
                {
                    At += 2;
                }
                else if((At[0] == 'l') && (At[1] == 'l'))
                {
                    At += 2;
                }
                else if(*At == 'h')
                {
                    ++At;
                }
                else if(*At == 'l')
                {
                    ++At;
                }
                else if(*At == 'j')
                {
                    ++At;
                }
                else if(*At == 'z')
                {
                    ++At;
                }
                else if(*At == 't')
                {
                    ++At;
                }
                else if(*At == 'L')
                {
                    ++At;
                }

                char TempBuffer[1024];
                char *Temp = TempBuffer;
                format_dest TempDest = {ArrayCount(TempBuffer), Temp};
                char *Prefix = "";
                b32 IsFloat = false;

                switch(*At)
                {
                    case 'd':
                    case 'i':
                    {
                        s64 Value = ReadVarArgSignedInteger(IntegerLength, &Args);
                        b32 WasNegative = (Value < 0);
                        if(WasNegative)
                        {
                            Prefix = "-";
                        }
                        U64ToASCII(&TempDest, (u64)Value, 10, DecChars);
                        if(WasNegative)
                        {
                            Prefix = "-";
                            OutChar(&TempDest, '-');
                        }
                        else if(ForceSign)
                        {
                            Assert(!PositiveSignIsBlank);
                            Prefix = "+";
                        }
                        else if(PositiveSignIsBlank)
                        {
                            Prefix = " ";
                        }
                    } break;

                    case 'u':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &Args);
                        U64ToASCII(&TempDest, Value, 8, DecChars);
                    } break;

                    case 'o':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &Args);
                        U64ToASCII(&TempDest, Value, 8, DecChars);
                        if(AnnotateIfNotZero && Value)
                        {
                            Prefix = "0";
                        }
                    } break;
                    
                    case 'x':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &Args);
                        U64ToASCII(&TempDest, Value, 16, LowerHexChars);
                        if(AnnotateIfNotZero && Value)
                        {
                            OutChars(&TempDest, "0x");
                        }
                    } break;
                    
                    case 'X':
                    {
                        u64 Value = ReadVarArgUnsignedInteger(IntegerLength, &Args);
                        U64ToASCII(&TempDest, Value, 16, UpperHexChars);
                        if(AnnotateIfNotZero && Value)
                        {
                            Prefix = "0X";
                        }
                    } break;
                    
                    case 'f':
                    case 'F':
                    case 'e':
                    case 'E':
                    case 'g':
                    case 'G':
                    case 'a':
                    case 'A':
                    {
                        f64 Value = ReadVarArgFloat(FloatLength, &Args);
                        F64ToASCII(&TempDest, Value, Precision);
                        IsFloat = true;
                    } break;
                    
                    case 'c':
                    {
                        char Value = (char)va_arg(Args, int);
                        OutChar(&TempDest, (char)Value);
                    } break;
                    
                    case 's':
                    {
                        char *String = va_arg(Args, char *);

                        if(PrecisionSpecified)
                        {
                            TempDest.Size = 0;
                            for(char *Scan = String;
                                *Scan && (TempDest.Size < Precision);
                                ++Scan)
                            {
                                ++TempDest.Size;
                            }
                        }
                        else
                        {
                            TempDest.Size = StringLength(String);
                        }

                        Temp = String;
                        TempDest.At = String + TempDest.Size;
                    } break;
                    
                    case 'S':
                    {
                        wchar_t *String = va_arg(Args, wchar_t *);

                        if(PrecisionSpecified)
                        {
                            TempDest.Size = 0;
                            for(wchar_t *Scan = String;
                                *Scan && (TempDest.Size < Precision);
                                ++Scan)
                            {
                                ++TempDest.Size;
                            }
                        }
                        else
                        {
                            TempDest.Size = StringLength(String);
                        }

                        Temp = UnwidenChars(String);
                        TempDest.At = Temp + TempDest.Size;
                    } break;
                    
                    case 'p':
                    {
                        void *Value = va_arg(Args, void *);
                        U64ToASCII(&TempDest, *(umm *)&Value, 16, LowerHexChars);
                    } break;
                    
                    case 'n':
                    {
                        int *TabDest = va_arg(Args, int *);
                        *TabDest = (int)(Dest.At - DestInit);
                    } break;
                    
                    case '%':
                    {
                        OutChar(&Dest, '%');
                    } break;
                    
                    default:
                    {
                        Assert(!"Unrecognized format specifier");
                    } break;
                }

                if(TempDest.At - Temp)
                {
                    smm UsePrecision = Precision;
                    if(IsFloat || !PrecisionSpecified)
                    {
                        UsePrecision = (TempDest.At - Temp);
                    }

                    smm PrefixLength = StringLength(Prefix);
                    smm UseWidth = Width;
                    smm ComputedWidth = UsePrecision + PrefixLength;
                    if(UseWidth < ComputedWidth)
                    {
                        UseWidth = ComputedWidth;
                    }

                    if(PadWithZeros)
                    {
                        Assert(!LeftJustify);
                        LeftJustify = false;
                    }

                    if(!LeftJustify)
                    {
                        while(UseWidth > (UsePrecision + PrefixLength))
                        {
                            OutChar(&Dest, PadWithZeros ? '0' : ' ');
                            --UseWidth;
                        }
                    }

                    for(char *Pre = Prefix;
                        *Pre && UseWidth;
                        ++Pre)
                    {
                        OutChar(&Dest, *Pre);
                        --UseWidth;
                    }

                    if(UsePrecision > UseWidth)
                    {
                        UsePrecision = UseWidth;
                    }
                    while(UsePrecision > (TempDest.At - Temp))
                    {
                        OutChar(&Dest, '0');
                        --UsePrecision;
                        --UseWidth;
                    }
                    while(UsePrecision && (TempDest.At != Temp))
                    {
                        OutChar(&Dest, *Temp++);
                        --UsePrecision;
                        --UseWidth;
                    }

                    if(LeftJustify)
                    {
                        while(UseWidth)
                        {
                            OutChar(&Dest, ' ');
                            --UseWidth;
                        }
                    }
                }

                if(*At)
                {
                    ++At;
                }
            }
            else
            {
                OutChar(&Dest, *At++);
            }
        }

        if(Dest.Size)
        {
            *Dest.At = 0;
        }
        else
        {
            Dest.At[-1] = 0;
        }
    }

    umm Result = Dest.At - DestInit;
    return(Result);
}

static umm
FormatString(umm DestSize, char *Dest, char *Format, ...)
{
    va_list Args;

    va_start(Args, Format);
    umm Result = FormatStringList(DestSize, Dest, Format, Args);
    va_end(Args);

    return(Result);
}


static smm
GetLastCharIndexInString(char *String, char Char)
{
    char *P = String + StringLength(String);
    while(P > String && *P != Char)
    {
        --P;
    }
    smm Result = -1;
    if(*P == Char)
    {
        Result = (P - String);
    }
    return(Result);
}

static smm
GetLastCharIndexInString(wchar_t *String, wchar_t Char)
{
    wchar_t *P = String + StringLength(String);
    while(P > String && *P != Char)
    {
        --P;
    }
    smm Result = -1;
    if(*P == Char)
    {
        Result = (P - String);
    }
    return(Result);
}
