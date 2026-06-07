/*
 * test_core.c — aggregator translation unit.
 *
 * Individual test areas live in their own test_*.c files (registered via
 * constructors in test_util.h). This file exists so the test target has a
 * stable anchor and a place for small cross-cutting smoke checks. The real
 * main() is in test_main.c.
 */
#include "test_util.h"

TEST(core_sanity) {
    CHECK(1 + 1 == 2);
    CHECK_EQ_INT(4, 2 * 2);
    CHECK_STR_EQ("xs", "xs");
}
