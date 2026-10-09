#include <libnx/libnx.hpp>
#include <chrono>
#include <thread>
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

int libnx_init(void) {
    // Initialize host-side subsystems
    // This is where the Switch's sm::Initialize, ns:am, etc. would map to
    return 0;
}

void libnx_exit(void) {
    // Cleanup
}

int libnx_get_os_version(void) {
    // In Switch this returns the firmware version
    // On host we return 0 or map to host kernel version
    return 0x0D0000;
}

const char* libnx_get_username(uint32_t user_id) {
    (void)user_id;
    static const char* default_user = "Player 1";
    return default_user;
}

int libnx_open(const char* path, int flags, int mode) {
    return ::open(path, flags, mode);
}

int libnx_close(int fd) {
    return ::close(fd);
}

ssize_t libnx_read(int fd, void* buf, size_t count) {
    return ::read(fd, buf, count);
}

ssize_t libnx_write(int fd, const void* buf, size_t count) {
    return ::write(fd, buf, count);
}

int64_t libnx_seek(int fd, int64_t offset, int whence) {
    return ::lseek(fd, offset, whence);
}

int libnx_thread_create(void* handle, void (*func)(void*), void* arg, size_t stack_size) {
    (void)stack_size;
    auto* pthread_handle = static_cast<pthread_t*>(handle);
    struct ThreadWrapper {
        void (*func)(void*);
        void* arg;
    };
    ThreadWrapper* wrapper = new ThreadWrapper{func, arg};
    int result = pthread_create(pthread_handle, nullptr, [](void* p) -> void* {
        ThreadWrapper* w = static_cast<ThreadWrapper*>(p);
        w->func(w->arg);
        delete w;
        return nullptr;
    }, wrapper);
    return result;
}

void libnx_thread_exit(int code) {
    pthread_exit(reinterpret_cast<void*>(static_cast<intptr_t>(code)));
}

int libnx_mutex_init(void* mutex) {
    return pthread_mutex_init(static_cast<pthread_mutex_t*>(mutex), nullptr);
}

int libnx_mutex_lock(void* mutex) {
    return pthread_mutex_lock(static_cast<pthread_mutex_t*>(mutex));
}

int libnx_mutex_unlock(void* mutex) {
    return pthread_mutex_unlock(static_cast<pthread_mutex_t*>(mutex));
}

void libnx_sleep(uint64_t nanoseconds) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

int libnx_get_timestamp(void) {
    return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
