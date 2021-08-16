#if !defined(FILES_H)

struct file
{
    wchar_t *Path;
    int PathLength;
    int Size;
    b32 IsDirectory;

    file *Next;
    file *Prev;
};

struct file_listing
{
    int Count;
    file *Head;
    file *Tail;
};

// TODO(chuck): Use file instead?
struct find_first_file
{
    wchar_t Path[1024];
    int PathLength;
    int PathFilenameIndex;
};

struct recursive_copy
{
    int FilesCreated;
    int DirectoriesCreated;
};

struct get_directory_for_file
{
    umm CharsWritten;
    char* Error;
};

struct path
{
    wchar_t *Data;
    umm Length;
};

struct win32_volume
{
    wchar_t *GUID;          /* NOTE(chuck): e.g. \\?\Volume{abcdefgh-0000-0000-0000-100000000000}\ */
    wchar_t *DeviceName;    /* NOTE(chuck): e.g. \Device\HarddiskVolume2 */
    wchar_t *Drive;         /* NOTE(chuck): e.g. C:\ */
    umm DriveLength;
};

struct win32_volume_list
{
    win32_volume Volume[10];
    umm Count;
};

static int GetFiles(wchar_t *Path, file_listing *FileListing);
static b32 FileExists(wchar_t *Path);
static wchar_t *GetFilename(wchar_t *Path);
static int CreateDirectory(wchar_t *Path);
static void DeleteFilesRecursively(wchar_t *Directory);
static int CreateDirectoriesRecursively(wchar_t *RootDirectory, wchar_t *RelativePath);
static recursive_copy CopyFilesRecursively(wchar_t *FromFolder, wchar_t *ToFolder);
static path FullPath(wchar_t *Path, umm PathLength,
                     wchar_t *FullRootDirectory, umm FullRootDirectoryLength,
                     wchar_t *ScratchBuffer, umm ScratchBufferLength);
static find_first_file FindFirstFileMatchInPathList(wchar_t *FileToFind, wchar_t *PathList);
static win32_volume_list Win32GetVolumeList();
static win32_volume *Win32GetVolumeForPath(wchar_t *Path);

#define FILES_H
#endif