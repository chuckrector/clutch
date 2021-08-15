# Clutch v1

Clutch is a "`cl.exe` snapshotter" which creates Microsoft Visual Studio compiler/linker distributions that are suitable for source control check-in. It was designed to collect the absolute minimum set of dependencies required to build your program at a given moment in time. It is able to know this by running your build script and observing all file I/O.

Clutch will copy `cl.exe` and `link.exe` and any other files accessed (for reading) into a target directory. The default target is a subdirectory of the current directory, named `cl`. Within the target directory, all `.h` and `.lib` files are copied into `include` and `lib` subdirectories respectively. All other files will be merged into the root directory.

# Usage

For example:

```shell
clutch build.bat portable_cl
```

The above invocation assumes that you are located in your project's root directory, that `build.bat` is your build script which is also located in the root directory, and that `portable_cl` is the name of the desired target subdirectory within that root. It will create the following structure:

```
portable_cl\
    cl.exe
    link.exe
    ...
    include\
        ...
    lib\
        ...
```

One example of using this generated portable distribution without modifying your environment would be to change your `build.bat` like so:

- Use `portable_cl\cl` instead of `cl`.
- Add `/Iportable_cl\include` to your compiler options.
- Add `/libpath:portable_cl\lib` to your linker options.

For more details, run `clutch` without any arguments.

# Notes

- `clutch` currently assumes 64-bit.
- Due to the minimalistic nature of taking a snapshot of dependencies at a given moment in time, adding new `.h` or `.lib` files which did not exist in your project at the time of the snapshot will of course change the set of files depended upon, sometimes drastically. As an example, changing whether you depend upon the CRT or not. If you are not currently exercising resources which you expect you may eventually require, referencing them in your project at the time of the snapshot may save you from needing to take additional snapshots.
- `clutch` deletes all the files and directories inside the target directory prior to each run.

# References

- [Event Tracing for Windows](https://docs.microsoft.com/en-us/windows/win32/etw/about-event-tracing)
- [THE WORST API EVER MADE](https://caseymuratori.com/blog_0025)
- [Handmade Hero Day 001 - Setting Up the Windows Build @ 47m25s](https://www.youtube.com/watch?v=Ee3EtYb8d1o&t=2845s)
