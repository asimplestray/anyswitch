# AnySwitch System Libraries

This directory contains host reimplementations of Nintendo Switch system libraries,
compiled as `.prx` shared libraries for dynamic linking.

## Structure

- `libnx/` — Nintendo Switch libnx runtime reimplementation (MIT)
- `libc/` — Host libc compatibility layer (malloc, stdio, string, etc.)
- `libkernel/` — Kernel svc calls mapped to POSIX (pthreads, sockets, file I/O)

Each directory contains:
- `CMakeLists.txt` - Build configuration
- `Export.cpp` - Symbol export stubs (Switch resolves imports by symbol name)
- `include/` - Header files
- `src/` - Implementation
- `tests/` - Unit tests

## Building

```bash
cmake --build build --target libnx libc libkernel
```

## Runtime Layout

```
output/
  libs/
    libnx.prx
    libc.prx
    libkernel.prx
    ...
  app0/
    game_resources/
    atmosphere/  (or reinx/)
      exefs/
        main.nso (already translated)
```
