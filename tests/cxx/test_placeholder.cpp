#include <gtest/gtest.h>

// Trivial smoke test — validates the cmake → ctest pipeline end-to-end.
// Real IPC/protocol tests live in tests/python/ via pytest.

TEST(Placeholder, OnePlusOne)
{
    EXPECT_EQ(1 + 1, 2);
}
