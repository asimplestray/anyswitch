#include "libkernel/libkernel.hpp"
#include "prx/libc/include/General.hpp"
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <iostream>

extern "C" {

int64_t svcOpenDir(const char* path) {
    return static_cast<int64_t>(::open(path, O_RDONLY | O_DIRECTORY));
}

int64_t svcOpenFile(const char* path, uint32_t flags) {
    return static_cast<int64_t>(::open(path, static_cast<int>(flags)));
}

int64_t svcCloseHandle(uint64_t handle) {
    return static_cast<int64_t>(::close(static_cast<int>(handle)));
}

int64_t svcReadFile(uint64_t handle, void* buf, size_t size) {
    return static_cast<int64_t>(::read(static_cast<int>(handle), buf, size));
}

int64_t svcWriteFile(uint64_t handle, const void* buf, size_t size) {
    return static_cast<int64_t>(::write(static_cast<int>(handle), buf, size));
}

int64_t svcCreateProcess(const char* name, void* prog, size_t size) {
    (void)name; (void)prog; (void)size;
    throw std::runtime_error("svcCreateProcess not fully implemented");
}

int64_t svcStartProcess(uint64_t handle, int32_t priority, int32_t core_mask, uint32_t stack_size) {
    (void)handle; (void)priority; (void)core_mask; (void)stack_size;
    NotImplemented_asw_stub("svcStartProcess");
    return -1;
}

int64_t svcExitProcess(void) {
    std::exit(0);
}

int64_t svcCreateThread(uint64_t* handle, void (*func)(void*), void* arg, void* stack_base, int32_t stack_size, int32_t priority, int32_t core) {
    (void)stack_base; (void)stack_size; (void)priority; (void)core;
    auto* pthread_handle = reinterpret_cast<pthread_t*>(handle);
    struct Wrapper { void (*func)(void*); void* arg; };
    auto* w = new Wrapper{func, arg};
    int result = pthread_create(pthread_handle, nullptr, [](void* p) -> void* {
        auto w = static_cast<Wrapper*>(p);
        w->func(w->arg);
        delete w;
        return nullptr;
    }, w);
    return result == 0 ? 0 : -1;
}

int64_t svcExitThread(void) {
    pthread_exit(nullptr);
    return 0;
}

int64_t svcSleepThread(uint64_t nanoseconds) {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
    return 0;
}

int64_t svcSignalEvent(uint64_t handle) {
    NotImplemented_asw_stub("svcSignalEvent");
    return -1;
}

int64_t svcWaitEvent(uint64_t handle, uint64_t* out, int64_t timeout) {
    NotImplemented_asw_stub("svcWaitEvent");
    (void)handle; (void)out; (void)timeout;
    return -1;
}

int64_t svcMapMemory(void* addr, size_t size) {
    (void)addr; (void)size;
    NotImplemented_asw_stub("svcMapMemory");
    return -1;
}

int64_t svcUnmapMemory(void* addr, size_t size) {
    (void)addr; (void)size;
    NotImplemented_asw_stub("svcUnmapMemory");
    return -1;
}

int64_t svcAllocateMemory(size_t size, uint32_t perm) {
    (void)size; (void)perm;
    NotImplemented_asw_stub("svcAllocateMemory");
    return -1;
}

} // extern "C"
