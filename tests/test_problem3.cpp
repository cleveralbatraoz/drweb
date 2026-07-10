extern "C"
{
#include "problem3.h"
}

#include <catch_amalgamated.hpp>
#include <climits>

extern "C" int f_asm(int x); // problem3.asm

TEST_CASE("boundaries", "[problem3]")
{
    CHECK(f(0) == 0);
    for (const int x : {1, -1, 2, -2, 42, INT_MIN, INT_MAX, 0x40000000})
    {
        CHECK(f(x) == 1);
    }
}

TEST_CASE("asm parity", "[problem3]")
{
    for (int x = -65536; x <= 65536; ++x)
    {
        REQUIRE(f(x) == f_asm(x));
    }
    for (const int x : {INT_MIN, INT_MIN + 1, INT_MAX - 1, INT_MAX})
    {
        CHECK(f(x) == f_asm(x));
    }
}
