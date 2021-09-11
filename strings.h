#if !defined(STRINGS_H)

static char DecChars[] = "0123456789";
static char LowerHexChars[] = "0123456789abcdef";
static char UpperHexChars[] = "0123456789ABCDEF";

struct wide_buffer
{
    int MaxCount;
    int Count;
    wchar_t *Data;
};
typedef struct wide_buffer wide_buffer;

struct format_dest
{
    umm Size;
    char *At;
};
typedef struct format_dest format_dest;

static int StringLengthC(char *String);
static int StringLengthW(wchar_t *String);
static void CopyStringCC(char *To, char *From);
static void CopyStringWW(wchar_t *To, wchar_t *From);
static b32 IsCRLF(char *P);
static b32 IsLF(char *P);
static b32 IsCharEqual(wchar_t A, wchar_t B);
static b32 StringEndsWith(wchar_t *String, wchar_t *Suffix);
static b32 StringsAreEqualCC(char *A, char *B);
static b32 StringsAreEqualWW(wchar_t *A, wchar_t *B);
static b32 StringStartsWith(wchar_t *String, wchar_t *Prefix);
static b32 StringsAreEqualWithinLength(wchar_t *A, wchar_t *B, umm Size);
static b32 StringContains(wchar_t *String, wchar_t *Substring);
static b32 IsEndComment(char *P);
static wchar_t *WidenChars(char *String);
static char *UnwidenChars(wchar_t *String);
static void AppendChar(wide_buffer *Buffer, wchar_t Char);
static void AppendString(wide_buffer *Buffer, wchar_t *String);
static int AppendDecimal(wide_buffer *Buffer, u32 Value);
static void AppendDouble(wide_buffer *Buffer, double Value, u32 Precision);
static void OutChar(format_dest *Dest, char Value);
static void OutChars(format_dest *Dest, char *String);
static s32 S32FromZInternal(char **AtInit);
static s32 S32FromZ(char *At);
static u64 ReadVarArgSignedInteger(u32 Length, va_list *Args);
static u64 ReadVarArgUnsignedInteger(u32 Length, va_list *Args);
static f64 ReadVarArgFloat(u32 Length, va_list *Args);
static void U64ToASCII(format_dest *Dest, u64 Value, u32 Base, char *Digits);
static void F64ToASCII(format_dest *Dest, f64 Value, u32 Precision);
static umm FormatStringList(umm DestSize, char *DestInit, char *Format, va_list Args);
static umm FormatString(umm DestSize, char *Dest, char *Format, ...);
static smm GetLastCharIndexInStringCC(char *String, char Char);
static smm GetLastCharIndexInStringWW(wchar_t *String, wchar_t Char);

#define STRINGS_H
#endif