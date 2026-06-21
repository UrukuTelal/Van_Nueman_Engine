#ifndef VN_SCALED_INT_TOOLCHAIN_H
#define VN_SCALED_INT_TOOLCHAIN_H

// Toolchain wrapper: define _MSC_VER so ScaledInt.h skips long long constructors
// that conflict with ScaledInt(T raw) when T=long long (int64_t on Windows).
// ugc's Clang does not set _MSC_VER by default.
#ifndef _MSC_VER
#define _MSC_VER 1920
#define _VN_TOOLCHAIN_MSC_VER_DEFINED
#endif

#include "../../../../vn/ScaledInt.h"

#ifdef _VN_TOOLCHAIN_MSC_VER_DEFINED
#undef _MSC_VER
#undef _VN_TOOLCHAIN_MSC_VER_DEFINED
#endif

#endif // VN_SCALED_INT_TOOLCHAIN_H
