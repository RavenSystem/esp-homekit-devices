#ifndef _TESTCASE_H
#define _TESTCASE_H
#include <stdbool.h>
#include <stdio.h>
#include "esp/uart.h"

/* Unity is the framework with test assertions, etc. */
#include "unity.h"

/* Need to explicitly flag once a test has completed successfully. */
#undef TEST_PASS
#define TEST_PASS() do { UnityConcludeTest(); while(1) { } } while (0)

/* Types of test, defined by hardware requirements */
typedef enum {
    SOLO,        /* Test require "ESP A" only, no other connections */
    DUAL,        /* Test requires "ESP A" and "ESP "B", basic interconnections between them */
    EYORE_TEST,  /* Test requires an eyore-test board with onboard STM32F0 */
} testcase_type_t;

typedef void (testcase_fn_t)(void);

typedef struct {
    const char *name;
    const char *file;
    int line;
    testcase_type_t type;
    testcase_fn_t *a_fn;
    testcase_fn_t *b_fn;
} testcase_t;

void testcase_register(const testcase_t *testcase);

/* Register a test case using these macros. Use DEFINE_SOLO_TESTCASE for single-MCU tests,
   and DEFINE_TESTCASE for all other test types.
*/
#define DEFINE_SOLO_TESTCASE(NAME)                                      \
    static testcase_fn_t a_##NAME;                                      \
    _DEFINE_TESTCASE_COMMON(NAME, SOLO, a_##NAME, 0)

#define DEFINE_TESTCASE(NAME, TYPE)                                     \
    static testcase_fn_t a_##NAME;                                      \
    static testcase_fn_t b_##NAME;                                      \
    _DEFINE_TESTCASE_COMMON(NAME, TYPE, a_##NAME, b_##NAME)


#define _DEFINE_TESTCASE_COMMON(NAME, TYPE, A_FN, B_FN)                 \
    void __attribute__((constructor)) testcase_ctor_##NAME() {          \
        const testcase_t testcase = { .name = #NAME,                    \
                                      .file = __FILE__,                 \
                                      .line = __LINE__,                 \
                                      .type = TYPE,                     \
                                      .a_fn = A_FN,                     \
                                      .b_fn = B_FN,                     \
        };                                                              \
        testcase_register(&testcase);                                   \
    }

#endif
