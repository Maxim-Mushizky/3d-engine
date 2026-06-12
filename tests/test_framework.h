#pragma once

// Minimal dependency-free test harness. We can't use <cassert> because the
// release preset builds with NDEBUG, which compiles assert() to a no-op — tests
// must fail loudly regardless of build type. Each CHECK logs on failure and
// increments a global counter; main() returns that counter so a non-zero exit
// fails the CTest run (and therefore the CI job).

#include <cmath>
#include <cstdio>

namespace forge::test {

inline int g_failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("[FAIL] %s:%d  CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            ++::forge::test::g_failures;                                         \
        }                                                                        \
    } while (0)

// Float comparison with an absolute tolerance — geometry math is approximate.
inline bool ApproxEq(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

} // namespace forge::test
