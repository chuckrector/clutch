@echo off

setlocal

REM
REM 4100 unreferenced formal parameter
REM 4101 unreferenced local variable
REM 4127 conditional expression is constant
REM 4189 local variable is initialized but not referenced
REM 4200 nonstandard extension used
REM 4505 unreferenced local function has been removed
REM 4576 parenthesized type followed by initializer list is non-standard explicit type conversion syntax
REM 4702 unreachable code
REM 4706 assignment within conditional expression
REM 4996 deprecated/unsafe
REM
set CompilerFlags=/Gm- /GR- /GS- /Gs999999 /nologo /WX /W4 /wd4100 /wd4101 /wd4189 /wd4200 /wd4505 /wd4576 /wd4702 /wd4706 /wd4996 /FC /Od /Z7
set LinkerFlags=/incremental:no /machine:x64 /manifest:no /opt:ref /subsystem:console

REM https://superuser.com/questions/160702/get-current-folder-name-by-a-dos-command
for %%I in (.) do set CurrentFolderName=%%~nxI

if not exist "build" mkdir build
pushd build

REM NOTE(chuck): start /b allows parallel compilation
start /b cl %CompilerFlags% ..\%CurrentFolderName%.cpp /link %LinkerFlags% advapi32.lib
start /b cl %CompilerFlags% ..\tests.cpp /link %LinkerFlags%

