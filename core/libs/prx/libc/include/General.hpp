#pragma once

#include <stdexcept>
#include <filesystem>
#include "general/VabiMacros.hpp"

extern "C" void NotImplemented_asw_stub(const char* funcName);
extern "C" void CxaFinalize_asw_stub(void* dso_handle);

#define ANYSWITCH_DUMMY_FUN \
int DummyFunction_asw_stub() { \
    NotImplemented_asw_stub(__func__); \
    return 0; \
}
