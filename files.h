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

#define FILES_H
#endif