#ifndef LIBS_PRX_LIBKERNEL_LIBKERNEL_HPP
#define LIBS_PRX_LIBKERNEL_LIBKERNEL_HPP

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// svc wrappers (simplified - maps to host kernel calls)
int64_t svcOpenDir(const char* path);
int64_t svcOpenFile(const char* path, uint32_t flags);
int64_t svcCloseHandle(uint64_t handle);
int64_t svcReadFile(uint64_t handle, void* buf, size_t size);
int64_t svcWriteFile(uint64_t handle, const void* buf, size_t size);
int64_t svcCreateProcess(const char* name, void* prog, size_t size);
int64_t svcStartProcess(uint64_t handle, int32_t priority, int32_t core_mask, uint32_t stack_size);
int64_t svcExitProcess(void);

// Thread/event services
int64_t svcCreateThread(uint64_t* handle, void (*func)(void*), void* arg, void* stack_base, int32_t stack_size, int32_t priority, int32_t core);
int64_t svcExitThread(void);
int64_t svcSleepThread(uint64_t nanoseconds);
int64_t svcSignalEvent(uint64_t handle);
int64_t svcWaitEvent(uint64_t handle, uint64_t* out, int64_t timeout);

// Memory
int64_t svcMapMemory(void* addr, size_t size);
int64_t svcUnmapMemory(void* addr, size_t size);
int64_t svcAllocateMemory(size_t size, uint32_t perm);

#ifdef __cplusplus
}
#endif

#endif
