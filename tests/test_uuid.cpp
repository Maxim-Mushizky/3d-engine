#include "test_framework.h"

#include "forge/core/UUID.h"

namespace forge::test {

// 0 is the reserved "no entity" sentinel, so a generated id must never be 0,
// and two consecutive draws must differ (the generator is a 64-bit PRNG).
void RunUuidTests()
{
    UUID a = GenerateUUID();
    UUID b = GenerateUUID();
    CHECK(a != 0);
    CHECK(b != 0);
    CHECK(a != b);
}

} // namespace forge::test
