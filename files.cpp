/*
NOTE(chuck): A general rule about files vs directories:
    - Any path with a trailing backslash is always treated as a directory.
    - Any path without a trailing backslash is always treated as a file,
      even if a directory of the same name exists.
*/

static int
GetFiles(wchar_t *Path, file_listing *FileListing)
{
    int FilesFound = 0;
    Assert(Path[1] == ':'); // NOTE(chuck): Must be a full path
    // printf("GetDirectoryListing: %S\n", Path);

    int PathLength = StringLength(Path);
    wchar_t *SearchPath = PushArray(PathLength + 2 + 1, wchar_t);
    CopyString(SearchPath, Path);
    CopyString(SearchPath + PathLength, L"\\*");

    WIN32_FIND_DATAW FindData;
    HANDLE Handle = FindFirstFileW(SearchPath, &FindData);
    if(Handle != INVALID_HANDLE_VALUE)
    {
        for(;;)
        {
            b32 IsIgnore =
                StringsAreEqual(FindData.cFileName, L".") ||
                StringsAreEqual(FindData.cFileName, L"..");
            if(!IsIgnore)
            {
                file *File = FileListing->Tail;
                int FilePathLength = StringLength(FindData.cFileName);
                File->PathLength = FilePathLength;
                File->Path = PushArray(PathLength + FilePathLength + 2, wchar_t); // NOTE(chuck): \ and \0
                CopyString(File->Path, Path);
                File->Path[PathLength] = '\\';
                CopyString(File->Path + PathLength + 1, FindData.cFileName);
                File->Size = FindData.nFileSizeLow | ((u64)FindData.nFileSizeHigh << 32);
                File->IsDirectory = FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

                File->Next = PushStruct(file);
                File->Next->Prev = File;
                FileListing->Tail = File->Next;
                ++FileListing->Count;

                if(File->IsDirectory)
                {
                    FilesFound += GetFiles(File->Path, FileListing);
                }
            }
            
            //printf("%S\n", FindData.cFileName);
            b32 FindResult = FindNextFileW(Handle, &FindData);
            if(!FindResult)
            {
                if(GetLastError() != ERROR_NO_MORE_FILES)
                {
                    Printf(STD_ERROR_HANDLE, "Error finding files.  Error code: %d\n", GetLastError());
                }
                break;
            }
        }
    }

    return(FilesFound);
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
            Quit("Failed to create directory: %S\n  Error code %d\n", Path, GetLastError());
        }
    }
    return(CreationCount);
}

static void
DeleteFilesRecursively(wchar_t *Directory)
{
    file_listing FileListing = {};
    FileListing.Head = 
    FileListing.Tail = PushStruct(file);
    GetFiles(Directory, &FileListing);
    Log("Removing %d existing files...\n", FileListing.Count);

    file *File = FileListing.Head;
    for(int Index = 0;
        Index < FileListing.Count;
        ++Index)
    {
        if(!File->IsDirectory)
        {
            if(!DeleteFileW(File->Path))
            {
                Quit("Failed to delete file: %S\n", File->Path);
            }
        }
        File = File->Next;
    }

    // NOTE(chuck): Remove all directories in the target. Nested directories always
    // come last, so delete them in reverse order.
    File = FileListing.Tail->Prev;
    for(int Index = 0;
        Index < FileListing.Count;
        ++Index)
    {
        if(File->IsDirectory)
        {
            if(!RemoveDirectoryW(File->Path))
            {
                Quit("Failed to remove directory: %S\n", File->Path);
            }
        }
        File = File->Prev;
    }
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
CopyFilesRecursively(wchar_t *FromFolder, wchar_t *ToFolder)
{
    // printf("CopyFolderRecursively %s -> %s\n", FromFolder, ToFolder);
    recursive_copy Result = {};

    b32 CreateDirectoryResult = CreateDirectory(ToFolder);
    if(CreateDirectoryResult || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ++Result.DirectoriesCreated;

        wchar_t *FromFolderWithoutStar = PushArray(1024, wchar_t);
        wchar_t *FromFolderWithoutStarP = FromFolderWithoutStar;
        wchar_t *P = FromFolder;
        while(*P && *P != '*') *FromFolderWithoutStarP++ = *P++;
        *FromFolderWithoutStarP = 0;

        WIN32_FIND_DATAW FindData;
        HANDLE FindHandle = FindFirstFileW(FromFolder, &FindData);
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
                        wchar_t* NestedFromFolder = PushArray(1024, wchar_t);
                        wchar_t *NestedFromFolderP = NestedFromFolder;
                        P = FromFolderWithoutStar;
                        while(*P)
                        {
                            *NestedFromFolderP++ = *P++;
                        }
                        P = FindData.cFileName;
                        while(*P)
                        {
                            *NestedFromFolderP++ = *P++;
                        }
                        *NestedFromFolderP++ = '\\';
                        *NestedFromFolderP++ = '*';
                        *NestedFromFolderP = 0;

                        wchar_t *NestedToFolder = PushArray(1024, wchar_t);
                        wchar_t *NestedToFolderP = NestedToFolder;
                        P = ToFolder;
                        while(*P) *NestedToFolderP++ = *P++;
                        *NestedToFolderP++ = '\\';
                        P = FindData.cFileName;
                        while(*P) *NestedToFolderP++ = *P++;
                        *NestedToFolderP = 0;

                        recursive_copy NestedResult = CopyFilesRecursively(NestedFromFolder, NestedToFolder);
                        Result.FilesCreated += NestedResult.FilesCreated;
                        Result.DirectoriesCreated += NestedResult.DirectoriesCreated;
                    }
                }
                else if(FindData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
                {
                    wchar_t *ToFile = PushArray(1024, wchar_t);
                    wchar_t *ToFileP = ToFile;
                    P = ToFolder;
                    while(*P) *ToFileP++ = *P++;
                    *ToFileP++ = '\\';
                    P = FindData.cFileName;
                    while(*P) *ToFileP++ = *P++;
                    *ToFileP = 0;

                    wchar_t *FromFile = PushArray(1024, wchar_t);
                    wchar_t *FromFileP = FromFile;
                    P = FromFolderWithoutStar;
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
                        Quit("Unable to copy %s to %s\n", FromFile, ToFile);
                    }
                }

                NextResult = FindNextFileW(FindHandle, &FindData);
            } while(NextResult);
        }
        else
        {
            Quit("Unable to find first file in folder: %s\n", FromFolder);
        }
    }
    else
    {
        Quit("Unable to create target folder: %s\n  Error code %d\n", ToFolder, GetLastError());
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
        Quit("GetFullPath: Failed to write chars.  Error code %d\n", GetLastError());
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
    wchar_t *P = DevicePath + StringLength(DevicePrefix);
    while(*P && *P != '\\') ++P;
    ++P;
    umm DeviceNameLength = (P - DevicePath);
    wchar_t *VolumePath = P; // NOTE(chuck): Points to foo
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
        Printf(STD_ERROR_HANDLE, "Failed to remap device path to drive path: %S\n");
        Printf(STD_ERROR_HANDLE, "  Volumes searched:\n");
        for(umm Index = 0;
            Index < VolumeList->Count;
            ++Index)
        {
            win32_volume *Volume = VolumeList->Volume + Index;
            Printf(STD_ERROR_HANDLE, "    %S (%S)\n", Volume->DeviceName, Volume->Drive);
        }
        Quit("");
    }

    return(Result);
}

static win32_volume_list
Win32GetVolumeList()
{
    win32_volume_list Result = {};

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
        Quit("Failed to find the first volume.  Error code %d\n", GetLastError());
    }

    do
    {
        umm VolumeNameLength = StringLength(VolumeName);
        win32_volume *Volume = Result.Volume + Result.Count;
        
        Volume->GUID = VolumeName;
        Volume->DeviceName = PushArray(1024, wchar_t);
        Volume->Drive = PushArray(1024, wchar_t);

        VolumeName[--VolumeNameLength] = 0; // NOTE(chuck): Mangle for QueryDosDevice's benefit
        DWORD DeviceNameBytesWritten = QueryDosDeviceW(VolumeName + 4, Volume->DeviceName, 1024);
        if(!DeviceNameBytesWritten)
        {
            Quit("Failed to query the DOS device: %S\n  Error code %d\n", VolumeName + 4, GetLastError());
        }
        VolumeName[VolumeNameLength++] = '\\';

        DWORD ReturnLength;
        BOOL OK = GetVolumePathNamesForVolumeNameW(Volume->GUID, Volume->Drive, 1024, &ReturnLength);
        if(!OK)
        {
            Quit("Failed to get volume path names for volume name: %S\n  Error code %d\n", VolumeName, GetLastError());
        }
        Volume->DriveLength = StringLength(Volume->Drive);

        ++Result.Count;
        if(Result.Count >= ArrayCount(Result.Volume))
        {
            Quit("Too many volumes.  Maximum allowed: %d\n", ArrayCount(Result.Volume));
        }

        VolumeName = PushArray(1024, wchar_t);
    } while(FindNextVolumeW(VolumeHandle, VolumeName, 1024));

    return(Result);
}
