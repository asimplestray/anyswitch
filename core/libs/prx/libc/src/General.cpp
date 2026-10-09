#include "General.hpp"
#include <iostream>
#include <cstdlib>

extern "C" void NotImplemented_asw_stub(const char* funcName) {
    std::cerr << "UNIMPLEMENTED: " << funcName << "\n";
    throw std::runtime_error(std::string(funcName) + " not implemented");
}

extern "C" void CxaFinalize_asw_stub(void* dso_handle) {
    (void)dso_handle;
    // C++ static destructor cleanup
}
