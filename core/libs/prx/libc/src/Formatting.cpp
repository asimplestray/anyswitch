#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FileStream.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" {

int ANYSWITCH_HOST_ABI snprintf_asw(char* buf, std::size_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = std::vsnprintf(buf, size, fmt, args);
    va_end(args);
    return result;
}

int ANYSWITCH_HOST_ABI vsnprintf_asw(char* buf, std::size_t size, const char* fmt, va_list args) {
    return std::vsnprintf(buf, size, fmt, args);
}

int ANYSWITCH_HOST_ABI fprintf_asw(Domain::FileStream* stream, const char* fmt, ...) {
    if (!stream) return -1;
    va_list args;
    va_start(args, fmt);
    int result = std::vfprintf(stream->GetHandle(), fmt, args);
    va_end(args);
    stream->SyncStatus();
    return result;
}

}
