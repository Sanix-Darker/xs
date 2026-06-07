#include "test_util.h"
#include <stdio.h>

/* Linked list of registered tests, built by constructors at load time. */
static XsTest *g_tests_head = NULL;
static XsTest *g_tests_tail = NULL;
int xs_test_total_failures = 0;

void xs_test_register(XsTest *t) {
    t->next = NULL;
    if (!g_tests_head) {
        g_tests_head = g_tests_tail = t;
    } else {
        g_tests_tail->next = t;
        g_tests_tail = t;
    }
}

int main(void) {
    int total = 0, passed = 0, failed = 0;

    for (XsTest *t = g_tests_head; t; t = t->next) {
        total++;
        t->failures = 0;
        t->fn(t);
        if (t->failures == 0) {
            passed++;
            printf("PASS %s\n", t->name);
        } else {
            failed++;
            printf("FAIL %s (%d check(s) failed)\n", t->name, t->failures);
        }
    }

    printf("\n==== %d tests: %d passed, %d failed ====\n", total, passed, failed);
    return failed == 0 ? 0 : 1;
}
