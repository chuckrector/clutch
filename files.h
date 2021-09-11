#if !defined(FILES_H)

struct file
{
    wchar_t *Path;
    int PathLength;
    int Size;
    b32 IsDirectory;
};
typedef struct file file;

struct file_listing
{
    int MaxCount;
    int Count;
    file *List;

    int FileCount;
    int DirectoryCount;
};
typedef struct file_listing file_listing;

// TODO(chuck): Use file instead?
struct find_first_file
{
    wchar_t Path[1024];
    int PathLength;
    int PathFilenameIndex;
};
typedef struct find_first_file find_first_file;

struct recursive_copy
{
    int FilesCreated;
    int DirectoriesCreated;
};
typedef struct recursive_copy recursive_copy;

struct get_directory_for_file
{
    umm CharsWritten;
    char* Error;
};
typedef struct get_directory_for_file get_directory_for_file;

struct path
{
    wchar_t *Data;
    umm Length;
};
typedef struct path path;

struct win32_volume
{
    wchar_t *GUID;          /* NOTE(chuck): e.g. \\?\Volume{abcdefgh-0000-0000-0000-100000000000}\ */
    wchar_t *DeviceName;    /* NOTE(chuck): e.g. \Device\HarddiskVolume2 */
    wchar_t *Drive;         /* NOTE(chuck): e.g. C:\ */
    umm DriveLength;
};
typedef struct win32_volume win32_volume;

struct win32_volume_list
{
    umm MaxCount;
    umm Count;
    win32_volume *Volume;
};
typedef struct win32_volume_list win32_volume_list;

static void GetFiles(wchar_t *Path, file_listing *FileListing);
static b32 FileExists(wchar_t *Path);
static wchar_t *GetFilename(wchar_t *Path);
static int CRR_CreateDirectory(wchar_t *Path);
static umm DeleteFilesRecursively(wchar_t *Directory);
static int CreateDirectoriesRecursively(wchar_t *RootDirectory, wchar_t *RelativePath);
static recursive_copy CopyFilesRecursively(wchar_t *FromFolder, wchar_t *ToFolder);
static find_first_file FindFirstFileMatchInPathList(wchar_t *FileToFind, wchar_t *PathList);
static win32_volume_list Win32GetVolumeList();
static win32_volume *Win32GetVolumeForPath(wchar_t *Path);

#define FILES_H
#endif