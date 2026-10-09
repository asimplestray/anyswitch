#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_FILESTREAM_HPP

#include <cstdio>
#include <cstdint>
#include <array>

namespace Domain {

using JumpBuf = std::array<std::uint64_t, 32>;

struct FileStream {
    FILE* handle;
    int fd = -1;

    FileStream(FILE* h) : handle(h) {}
    FileStream() : handle(nullptr) {}

    FILE* GetHandle() const { return handle; }
    void SetHandle(FILE* h) { handle = h; }
    int GetFd() const { return fd; }
    void SetFd(int f) { fd = f; }
    void SyncStatus() {}
    void SetEncodingError() {}
    bool Reopen(const char* path, const char* mode) {
        handle = std::fopen(path, mode);
        return handle != nullptr;
    }
};

}

#endif
