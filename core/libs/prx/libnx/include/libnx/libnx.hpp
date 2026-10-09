#ifndef LIBS_PRX_LIBNX_LIBNX_HPP
#define LIBS_PRX_LIBNX_LIBNX_HPP

#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    uint32_t attributes;
    uint64_t address;
    uint64_t size;
} LibnxModuleInfo;

// libnx function stubs - to be reimplemented on the host
int libnx_init(void);
void libnx_exit(void);
int libnx_get_os_version(void);
const char* libnx_get_username(uint32_t user_id);

// File system
int libnx_open(const char* path, int flags, int mode);
int libnx_close(int fd);
ssize_t libnx_read(int fd, void* buf, size_t count);
ssize_t libnx_write(int fd, const void* buf, size_t count);
int64_t libnx_seek(int fd, int64_t offset, int whence);

// Threading
int libnx_thread_create(void* handle, void (*func)(void*), void* arg, size_t stack_size);
void libnx_thread_exit(int code);
int libnx_mutex_init(void* mutex);
int libnx_mutex_lock(void* mutex);
int libnx_mutex_unlock(void* mutex);

// OS
void libnx_sleep(uint64_t nanoseconds);
int libnx_get_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif
