// Switch .NRO/.PRX export stubs
// Nintendo Switch games import functions via NRO symbol resolution
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {
    // Standard streams for the host
    int ANYSWITCH_HOST_ABI fgetc_asw(void* stream);
    int ANYSWITCH_HOST_ABI fputc_asw(int value, void* stream);
    void* ANYSWITCH_HOST_ABI fopen_asw(const char* path, const char* mode);
    int ANYSWITCH_HOST_ABI fclose_asw(void* stream);

    // File operations
    int64_t ANYSWITCH_HOST_ABI fseek_asw(void* stream, int64_t offset, int origin);
    int64_t ANYSWITCH_HOST_ABI ftell_asw(void* stream);
}
