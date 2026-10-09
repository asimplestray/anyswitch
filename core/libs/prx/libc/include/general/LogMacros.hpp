#pragma once
#include <cstring>

// Strips the AnySwitch `_asw` implementation suffix for log readability.
inline const char* TrimAswSuffix(const char* func) {
    return func;
}

constexpr const char* BaseName(const char* path) {
    const char* last = path;
    const char* prev = nullptr;
    for (const char* p = path; *p; ++p)
        if (*p == '/' || *p == '\\') {
            prev = last;
            last = p + 1;
        }
    return (prev && *prev) ? prev : last;
}

#define _ANYSWITCH_FILE_ BaseName(__FILE__)
