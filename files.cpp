/*
NOTE(chuck): A general rule about files vs directories:
    - Any path with a trailing backslash is always treated as a directory.
    - Any path without a trailing backslash is always treated as a file,
      even if a directory of the same name exists.
*/

#include "literals.h"

static void
GetFiles(wchar_t *Path, file_listing *FileListing)
{
    Assert(Path[1] == ':'); // NOTE(chuck): Must be a full path
    // printf("GetDirectoryListing: %S\n", Path);

    int PathLength = StringLength(Path);
    wchar_t *SearchingInHere = PushArray(PathLength + 2 + 1, wchar_t);
    CopyString(SearchingInHere, Path);
    CopyString(SearchingInHere + PathLength, L"\\*");

    WIN32_FIND_DATAW FindData;
    HANDLE Handle = FindFirstFileW(SearchingInHere, &FindData);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        for(;;)
        {
            b32 IsIgnore =
                StringsAreEqual(FindData.cFileName, L".") ||
                StringsAreEqual(FindData.cFileName, L"..");
            if(!IsIgnore)
            {
                if(FileListing->Count >= FileListing->MaxCount)
                {
                    Quit("Too many files were found -- over %d.\n%s", FileListing->MaxCount,
                         CHANGE_REQUIRES_RECOMPILE);
                }
                file *File = FileListing->List + FileListing->Count++;
                int FilePathLength = StringLength(FindData.cFileName);
                File->PathLength = FilePathLength;
                File->Path = PushArray(PathLength + FilePathLength + 2, wchar_t); // NOTE(chuck): \ and \0
                CopyString(File->Path, Path);
                File->Path[PathLength] = '\\';
                CopyString(File->Path + PathLength + 1, FindData.cFileName);
                File->Size = FindData.nFileSizeLow | ((u64)FindData.nFileSizeHigh << 32);
                File->IsDirectory = FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

                if(File->IsDirectory)
                {
                    ++FileListing->DirectoryCount;
                    GetFiles(File->Path, FileListing);
                }
                else
                {
                    ++FileListing->FileCount;
                }
            }
            
            //printf("%S\n", FindData.cFileName);
            b32 FindResult = FindNextFileW(Handle, &FindData);
            if(!FindResult)
            {
                if(GetLastError() != ERROR_NO_MORE_FILES)
                {
                    Quit("There was a problem finding files in the following path: %S\n", Path);
                }
                else
                {
                    break;
                }
            }
        }
    }
    else
    {
        Quit("There was a problem finding the first file in the following path: %S\n", Path);
    }
}

// NOTE(chuck): To avoid PathFileExistsW and dependency on shlwapi.lib
static b32
FileExists(wchar_t *Path)
{
    b32 Result = 0;
    WIN32_FIND_DATAW FindData;
    HANDLE Handle = FindFirstFileW(Path, &FindData);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        FindClose(Handle);
        Result = 1;
    }
    return(Result);
}

static wchar_t *
GetFilename(wchar_t *Path)
{
    wchar_t *Result = Path + StringLength(Path);
    while(*Result != '\\') --Result;
    Assert(*Result == '\\');
    ++Result;
    return(Result);
}

static int
CreateDirectory(wchar_t *Path)
{
    int CreationCount = 0;
    b32 Result = CreateDirectoryW(Path, 0);
    if(Result)
    {
        ++CreationCount;
    }
    else
    {
        if(GetLastError() != ERROR_ALREADY_EXISTS)
        {
            Quit("The following directory could not be created: %S\n", Path);
        }
    }
    return(CreationCount);
}

static umm
DeleteFilesRecursively(wchar_t *Directory)
{
    Assert(Directory);

    umm FilesDeleted = 0;

    file_listing FileListing = {1024*10, 0, PushArray(1024*10, file)};

    GetFiles(Directory, &FileListing);
    if(FileListing.Count)
    {
        // NOTE(chuck): ARE YOU NOT ENTERTAINED?!
        if(FileListing.Count == 1)
        {
            if(FileListing.List[0].IsDirectory)
            {
                Log("%d existing directory in %S is being deleted.\n", FileListing.Count, Directory);
            }
            else
            {
                Log("%d existing file in %S is being deleted.\n", FileListing.Count, Directory);
            }
        }
        else
        {
            if(FileListing.FileCount && FileListing.DirectoryCount)
            {
                Log("%d existing files and directories in %S are being deleted.\n", FileListing.Count, Directory);
            }
            else if(!FileListing.FileCount)
            {
                Log("%d existing directories in %S are being deleted.  There were no existing files.\n", FileListing.Count, Directory);
            }
            else
            {
                Log("%d existing files in %S are being deleted.  There were no existing directories.\n", FileListing.Count, Directory);
            }
        }
    }
    // Printf("Removing %d existing files in %S\n", FileListing.Count, Directory);
    for(int Index = 0;
        Index < FileListing.Count;
        ++Index)
    {
        file* File = FileListing.List + Index;
        if(!File->IsDirectory)
        {
            if(!DeleteFileW(File->Path))
            {
                Quit("The following file could not be deleted: %S\n", File->Path);
            }
            ++FilesDeleted;
        }
    }

    // NOTE(chuck): Remove all directories in the target. Nested directories always
    // come last, so delete them in reverse order.
    for(int Index = FileListing.Count - 1;
        Index >= 0;
        --Index)
    {
        file *File = FileListing.List + Index;
        if(File->IsDirectory)
        {
            if(!RemoveDirectoryW(File->Path))
            {
                Quit("The following directory could not be removed: %S\n", File->Path);
            }
            ++FilesDeleted;
        }
    }

    return(FilesDeleted);
}

// NOTE(chuck): RootDirectory is expected to be an absolute path.
static int
CreateDirectoriesRecursively(wchar_t *RootDirectory, wchar_t *RelativePath)
{
    Assert(*RelativePath != '\\');
    
    int CreationCount = 0;
    
    wchar_t *FullPath = PushArray(1024, wchar_t);
    int FullPathLength = 0;

    wchar_t *P = RootDirectory;
    wchar_t *Dest = FullPath;
    while(*P)
    {
        Assert(FullPathLength < 1024);
        *Dest++ = *P++;
        ++FullPathLength;
    }
    Assert(*P != '\\');
    *Dest++ = '\\';
    
    // NOTE(chuck): Append the relative path
    P = RelativePath;
    while(*P && *P != '\\')
    {
        Assert(FullPathLength < 1024);
        *Dest++ = *P++;
        ++FullPathLength;
    }
    
    if(*P == '\\')
    {
        CreationCount += CreateDirectory(FullPath);
        wchar_t *NextRelativePath = P + 1;
        CreationCount += CreateDirectoriesRecursively(FullPath, NextRelativePath);
    }
    
    return(CreationCount);
}

// TODO(chuck): I refactored all this hastily from a currently unused implementation
// that I was previously using.  Need to test this out and make sure it still works.
static recursive_copy
CopyFilesRecursively(wchar_t *FromDirectory, wchar_t *ToDirectory)
{
    // printf("CopyFolderRecursively %s -> %s\n", FromDirectory, ToDirectory);
    recursive_copy Result = {};

    b32 CreateDirectoryResult = CreateDirectory(ToDirectory);
    if(CreateDirectoryResult || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ++Result.DirectoriesCreated;

        wchar_t *FromDirectoryWithoutStar = PushArray(1024, wchar_t);
        wchar_t *FromDirectoryWithoutStarP = FromDirectoryWithoutStar;
        wchar_t *P = FromDirectory;
        while(*P && *P != '*') *FromDirectoryWithoutStarP++ = *P++;
        *FromDirectoryWithoutStarP = 0;

        WIN32_FIND_DATAW FindData;
        HANDLE FindHandle = FindFirstFileW(FromDirectory, &FindData);
        if(FindHandle != INVALID_HANDLE_VALUE)
        {
            b32 NextResult;
            b32 OK = true;
            do
            {
                if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    P = FindData.cFileName;
                    b32 IsDot    = (P[0] == '.') && !P[1];
                    b32 IsDotDot = (P[0] == '.') && (P[1] == '.') && !P[2];
                    if(!IsDot && !IsDotDot)
                    {
                        wchar_t* NestedFromDirectory = PushArray(1024, wchar_t);
                        wchar_t *NestedFromDirectoryP = NestedFromDirectory;
                        P = FromDirectoryWithoutStar;
                        while(*P)
                        {
                            *NestedFromDirectoryP++ = *P++;
                        }
                        P = FindData.cFileName;
                        while(*P)
                        {
                            *NestedFromDirectoryP++ = *P++;
                        }
                        *NestedFromDirectoryP++ = '\\';
                        *NestedFromDirectoryP++ = '*';
                        *NestedFromDirectoryP = 0;

                        wchar_t *NestedToDirectory = PushArray(1024, wchar_t);
                        wchar_t *NestedToDirectoryP = NestedToDirectory;
                        P = ToDirectory;
                        while(*P) *NestedToDirectoryP++ = *P++;
                        *NestedToDirectoryP++ = '\\';
                        P = FindData.cFileName;
                        while(*P) *NestedToDirectoryP++ = *P++;
                        *NestedToDirectoryP = 0;

                        recursive_copy NestedResult = CopyFilesRecursively(NestedFromDirectory, NestedToDirectory);
                        Result.FilesCreated += NestedResult.FilesCreated;
                        Result.DirectoriesCreated += NestedResult.DirectoriesCreated;
                    }
                }
                else if(FindData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
                {
                    wchar_t *ToFile = PushArray(1024, wchar_t);
                    wchar_t *ToFileP = ToFile;
                    P = ToDirectory;
                    while(*P) *ToFileP++ = *P++;
                    *ToFileP++ = '\\';
                    P = FindData.cFileName;
                    while(*P) *ToFileP++ = *P++;
                    *ToFileP = 0;

                    wchar_t *FromFile = PushArray(1024, wchar_t);
                    wchar_t *FromFileP = FromFile;
                    P = FromDirectoryWithoutStar;
                    while(*P) *FromFileP++ = *P++;
                    if(FromFileP[-1] != '\\') *FromFileP++ = '\\';
                    P = FindData.cFileName;
                    while(*P) *FromFileP++ = *P++;
                    *FromFileP = 0;
                    // printf("Copying %s to %s\n", FromFile, ToFile);
                    b32 CopyResult = CopyFileW(FromFile, ToFile, 0);
                    if(CopyResult)
                    {
                        ++Result.FilesCreated;
                    }
                    else
                    {
                        Quit("There was a problem copying `%s` to `%s`.\n", FromFile, ToFile);
                    }
                }

                NextResult = FindNextFileW(FindHandle, &FindData);
            } while(NextResult);
        }
        else
        {
            Quit("There was a problem finding the first file in the following directory: %s\n", FromDirectory);
        }
    }
    else
    {
        Quit("The following directory could not be created: %s\n", ToDirectory);
    }

    return(Result);
}

static wchar_t *
GetFullPath(wchar_t *Path)
{
    wchar_t *FullPath = PushArray(1024, wchar_t);
    DWORD CharsWritten = GetFullPathNameW(Path, 1024, FullPath, 0);
    if(!CharsWritten)
    {
        Quit("There was a problem getting the full path name for the following path:\n    %S\n", Path);
    }
    return(FullPath);
}

// NOTE(chuck): Parses a ;-delimited PATH and searches for a file in every every directory
// until it finds a match.
// TODO(chuck): Hastily refactored.  I don't currently use it but it needs to be tested.
static find_first_file
FindFirstFileMatchInPathList(wchar_t *FileToFind, wchar_t *PathList)
{
    find_first_file Result = {};

    wchar_t *P = PathList;
    while(*P && !Result.PathLength)
    {
        wchar_t CurrentFolder[1024];
        wchar_t *CurrentFolderP = CurrentFolder;
        while(*P && *P != ';')
        {
            *CurrentFolderP++ = *P++;
        }
        *CurrentFolderP = 0;
        if(*P == ';')
        {
            ++P;
        }
        wchar_t *Filename;
        Result.PathLength = SearchPathW(CurrentFolder, FileToFind, 0,
                                        sizeof(Result.Path), Result.Path,
                                        &Filename);
        if(Result.PathLength)
        {
            Result.PathFilenameIndex = (int)(Filename - Result.Path);
            break;
        }
    }

    return(Result);
}

static wchar_t *
Win32DevicePathToDrivePath(win32_volume_list *VolumeList, wchar_t *DevicePath)
{
    Assert(VolumeList);
    Assert(DevicePath);
    wchar_t *DevicePrefix = L"\\Device\\";
    Assert(StringStartsWith(DevicePath, DevicePrefix));

    // NOTE(chuck): In \Device\HarddiskVolume1\foo skip to foo
    umm DeviceNameLength;
    wchar_t *VolumePath;
    {
        wchar_t *P = DevicePath + StringLength(DevicePrefix);
        while(*P && *P != '\\') ++P;
        ++P;
        DeviceNameLength = (P - DevicePath);
        VolumePath = P; // NOTE(chuck): Points to foo
    }
    umm DevicePathLength = StringLength(DevicePath);
    umm VolumePathLength = DevicePathLength - DeviceNameLength;

    wchar_t *Result = 0;
    for(umm Index = 0;
        Index < VolumeList->Count;
        ++Index)
    {
        win32_volume *Volume = VolumeList->Volume + Index;
        if(StringStartsWith(DevicePath, Volume->DeviceName))
        {
            Result = PushArray(Volume->DriveLength + VolumePathLength + 1, wchar_t);
            CopyString(Result, Volume->Drive);
            CopyString(Result + Volume->DriveLength, VolumePath);
            break;
        }
    }

    if(!Result)
    {
        umm VolumesSearchedBufferSize = Kilobytes(2048);
        char *VolumesSearchedBuffer = PushArray(VolumesSearchedBufferSize, char);
        char *P = VolumesSearchedBuffer;
        umm BytesWritten = FormatString(VolumesSearchedBufferSize, P, "Volumes searched:\n");
        VolumesSearchedBufferSize -= BytesWritten;
        P += BytesWritten;

        for(umm Index = 0;
            Index < VolumeList->Count;
            ++Index)
        {
            win32_volume *Volume = VolumeList->Volume + Index;
            BytesWritten = FormatString(VolumesSearchedBufferSize, P, "    %S -> %S -> %S\n",
                                        Volume->GUID, Volume->DeviceName, Volume->Drive);
            VolumesSearchedBufferSize += BytesWritten;
            P += BytesWritten;
        }

        Quit("A mapping for the following device path could not be found:\n"
             "    %S\n%s", DevicePath, VolumesSearchedBuffer);
    }

    return(Result);
}

static win32_volume_list
Win32GetVolumeList()
{
    win32_volume_list Result = {16, 0, PushArray(16, win32_volume)};

    // NOTE(chuck): This is a full "volume GUID path".
    wchar_t *VolumeName = PushArray(1024, wchar_t);

    /*
    NOTE(chuck): FindFirstVolume etc. will return "volume GUID paths" which look like this:
        \\?\Volume{abcdefgh-0000-0000-0000-100000000000}\

    Device names can be looked up via QueryDosDevice by passing this part:
        Volume{abcdefgh-0000-0000-0000-100000000000}
    Which will yield things of the form:
        \Device\HarddiskVolume2

    Passing full volume GUIDs to GetVolumePathNamesForVolumeName gives the drive mappings, e.g.:
        C:\
    
    All of the paths in the events recorded by ETW use device names, e.g.:
        \Device\HarddiskVolume2\WINDOWS\System32\cmd.exe

    The volume list gathered here is useful for remapping device name paths to drive paths.
    */
    HANDLE VolumeHandle = FindFirstVolumeW(VolumeName, 1024);
    if(VolumeHandle == INVALID_HANDLE_VALUE)
    {
        Quit("There was a problem finding the first volume on your computer.\n");
    }

    do
    {
        umm VolumeNameLength = StringLength(VolumeName);
        if(Result.Count >= Result.MaxCount)
        {
            Quit("Too many volumes were found on your computer -- over %d.\n%s",
                Result.MaxCount, CHANGE_REQUIRES_RECOMPILE);
        }
        win32_volume *Volume = Result.Volume + Result.Count++;
        
        Volume->GUID = VolumeName;
        Volume->DeviceName = PushArray(1024, wchar_t);
        Volume->Drive = PushArray(1024, wchar_t);

        VolumeName[VolumeNameLength - 1] = 0;
        DWORD DeviceNameBytesWritten = QueryDosDeviceW(VolumeName + 4, Volume->DeviceName, 1024);
        if(!DeviceNameBytesWritten)
        {
            Quit("There was a problem getting the MS-DOS device name for the following volume:\n    %S\n", VolumeName + 4);
        }
        VolumeName[VolumeNameLength - 1] = '\\';

        DWORD ReturnLength;
        BOOL OK = GetVolumePathNamesForVolumeNameW(Volume->GUID, Volume->Drive, 1024, &ReturnLength);
        if(!OK)
        {
            Quit("There was a probelm getting the drive mappings for the following volume:\n    %S\n", VolumeName);
        }
        Volume->DriveLength = StringLength(Volume->Drive);

        VolumeName = PushArray(1024, wchar_t);
    } while(FindNextVolumeW(VolumeHandle, VolumeName, 1024));

    return(Result);
}
