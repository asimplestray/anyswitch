#pragma once

// Export alias: exposes funcName under exportName for dynamic linking.
// Switch games resolve imports by symbol name; the relinker rewrites
// .dynsym/.dynstr to point at these host implementations.
#if defined(__GNUC__) || defined(__clang__)
    #define ANYSWITCH_EXPORT(exportName, funcName) \
    __asm__(".globl \"" exportName "\"\n\t" \
    ".set \"" exportName "\", " #funcName)
#elif defined(_MSC_VER)
    #define ANYSWITCH_EXPORT(exportName, funcName) \
    __pragma(comment(linker, "/export:" exportName "=" #funcName))
#else
    #define ANYSWITCH_EXPORT(exportName, funcName)
#endif
