#ifndef LIBNX_EXPORTS_HPP
#define LIBNX_EXPORTS_HPP

#include <cstdint>

#ifdef _WIN32
#define NX_EXPORT __declspec(dllexport)
#else
#define NX_EXPORT __attribute__((visibility("default")))
#endif

#define NX_FUNC extern "C"

#endif
