#include "prx/libc/include/General.hpp"
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include "prx/libc/include/FileStream.hpp"

extern "C" {

Domain::FileStream* ANYSWITCH_HOST_ABI __stdinp_asw = new Domain::FileStream(stdin);
Domain::FileStream* ANYSWITCH_HOST_ABI __stdoutp_asw = new Domain::FileStream(stdout);
Domain::FileStream* ANYSWITCH_HOST_ABI __stderrp_asw = new Domain::FileStream(stderr);
int ANYSWITCH_HOST_ABI __isthreaded_asw = 1;

int ANYSWITCH_HOST_ABI fgetc_asw(Domain::FileStream* stream) {
    return stream ? std::fgetc(stream->GetHandle()) : EOF;
}

int ANYSWITCH_HOST_ABI fputc_asw(int ch, Domain::FileStream* stream) {
    return stream ? std::fputc(ch, stream->GetHandle()) : EOF;
}

int ANYSWITCH_HOST_ABI fclose_asw(Domain::FileStream* stream) {
    return stream ? std::fclose(stream->GetHandle()) : EOF;
}

Domain::FileStream* ANYSWITCH_HOST_ABI fopen_asw(const char* path, const char* mode) {
    FILE* f = std::fopen(path, mode);
    return f ? new Domain::FileStream(f) : nullptr;
}

int ANYSWITCH_HOST_ABI fflush_asw(Domain::FileStream* stream) {
    return stream ? std::fflush(stream->GetHandle()) : 0;
}

int ANYSWITCH_HOST_ABI feof_asw(Domain::FileStream* stream) {
    return stream ? std::feof(stream->GetHandle()) : -1;
}

}
