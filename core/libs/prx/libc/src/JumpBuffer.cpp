#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include <csetjmp>
#include <cstdlib>

extern "C" {

void ANYSWITCH_HOST_ABI jump_fclose_asw(Domain::JumpBuf env) {
    NotImplemented_asw_stub("jump_fclose");
}

}
