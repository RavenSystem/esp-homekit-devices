/**
 * Unit tests to verify the "unaligned load handler" in core/exception_vectors.S
 * that allows us to complete byte loads from unaligned memory, etc.
 *
 * Adapted from a test program in 'experiments' that did this.
 */
#include "testcase.h"
#include "esp/rom.h"
#include "esp/timer.h"
#include "esp/uart.h"
#include "espressif/esp_common.h"
#include "xtensa_ops.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"
#include "strings.h"

#include <malloc.h>

#define TESTSTRING "O hai there! %d %d %d"

static char dramtest[] = TESTSTRING;

static const __attribute__((section(".iram1.notrodata")))
    char iramtest[] = TESTSTRING;

static const __attribute__((section(".text.notrodata")))
    char iromtest[] = TESTSTRING;

static const volatile __attribute__((section(".iram1.notliterals")))
    int16_t unsigned_shorts[] = { -3, -4, -5, -32767, 44 };

static const __attribute__((section(".iram1.notrodata")))
    char sanity_test_data[] = {
        0x01, 0x55, 0x7e, 0x2a, 0x81, 0xd5, 0xfe, 0xaa
    };

DEFINE_SOLO_TESTCASE(03_byte_load_verify_sections)

#define PTR_IN_REGION(PTR, START, LEN) \
        ((START <= (intptr_t)(PTR)) && ((intptr_t)(PTR) < (START+LEN)))

/* Sanity check, ensure the addresses of the various test strings
 * are in the correct address space regions. */
static void a_03_byte_load_verify_sections()
{
  printf("dramtest addr %p\n", dramtest);
  TEST_ASSERT_MESSAGE(PTR_IN_REGION(dramtest, 0x3FFE8000, 0x14000),
          "dramtest should be in DRAM region");

  printf("iramtest addr %p\n", iramtest);
  TEST_ASSERT_MESSAGE(PTR_IN_REGION(iramtest, 0x40100000, 0x8000),
          "iramtest should be in IRAM region");

  printf("iromtest addr %p\n", iromtest);
  TEST_ASSERT_MESSAGE(PTR_IN_REGION(iromtest, 0x40202010, (0x100000 - 0x2010)),
          "iromtest sohuld be in IROM region");

  printf("unsigned_shorts addr %p\n", unsigned_shorts);
  TEST_ASSERT_MESSAGE(PTR_IN_REGION(unsigned_shorts, 0x40100000, 0x8000),
          "unsigned_shorts should be in IRAM region");

  printf("sanity_test_data addr %p\n", sanity_test_data);
  TEST_ASSERT_MESSAGE(PTR_IN_REGION(sanity_test_data, 0x40100000, 0x8000),
          "sanity_test_data should be in IRAM region");

  TEST_PASS();
}


/* test utility functions used for '03_byte_load_test_strings'

   returns the expected string result */
typedef const char *(* test_with_fn_t)(const char *string);

static char buf[64];

static const char * test_memcpy_aligned(const char *string)
{
    memcpy(buf, string, 16);
    return "O hai there! %d ";
}

static const char * test_memcpy_unaligned(const char *string)
{
    memcpy(buf, string, 15);
    return "O hai there! %d";
}


static const char * test_memcpy_unaligned2(const char *string)
{
    memcpy(buf, string+1, 15);
    return " hai there! %d ";
}

static const char * test_strcpy(const char *string)
{
    strcpy(buf, string);
    return dramtest;
}

static const char * test_sprintf(const char *string)
{
    sprintf(buf, string, 1, 2, 3);
    return "O hai there! 1 2 3";
}

static const char * test_sprintf_arg(const char *string)
{
    sprintf(buf, "%s", string);
    return dramtest;
}

static const char * test_naive_strcpy(const char *string)
{
    char *to = buf;
    while((*to++ = *string++))
        ;
    return dramtest;
}

static const char * test_naive_strcpy_a0(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a0, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a0, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a0, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a0", "a8", "a9");
    return dramtest;
}

static const char * test_naive_strcpy_a2(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a2, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a2, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a2, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a2", "a8", "a9");
    return dramtest;
}

static const char * test_naive_strcpy_a3(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a3, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a3, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a3, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a3", "a8", "a9");
    return TESTSTRING;
}

static const char * test_naive_strcpy_a4(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a4, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a4, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a4, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a4", "a8", "a9");
    return TESTSTRING;
}

static const char * test_naive_strcpy_a5(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a5, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a5, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a5, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a5", "a8", "a9");
    return TESTSTRING;
}

static const char * test_naive_strcpy_a6(const char *string)
{
    asm volatile (
"            mov          a8, %0    \n"
"            mov          a9, %1    \n"
"tns_loop%=: l8ui         a6, a9, 0 \n"
"            addi.n       a9, a9, 1 \n"
"            s8i          a6, a8, 0 \n"
"            addi.n       a8, a8, 1 \n"
"            bnez         a6, tns_loop%=\n"
        : : "r" (buf), "r" (string) : "a6", "a8", "a9");
    return TESTSTRING;
}

static const char * test_noop(const char *string)
{
  buf[0] = 0;
  return "";
}

static uint32_t IRAM inner_string_test(const char *string, test_with_fn_t testfn, const char *testfn_label, uint32_t nullvalue, bool evict_cache)
{
    printf(" .. against %30s: ", testfn_label);
    vPortEnterCritical();
    uint32_t before;
    RSR(before, CCOUNT);
    const int TEST_REPEATS = 1000;
    for(int i = 0; i < TEST_REPEATS; i++) {
      memset(buf, 0, sizeof(buf));
      const char *expected = testfn(string);
      TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, buf, testfn_label);
      if(evict_cache) {
        Cache_Read_Disable();
        Cache_Read_Enable(0,0,1);
      }
    }
    uint32_t after;
    RSR(after, CCOUNT);
    vPortExitCritical();
    uint32_t instructions = (after-before)/TEST_REPEATS - nullvalue;
    printf("%5d instructions\r\n", instructions);
    return instructions;
}

static void string_test(const char *string, char *label, bool evict_cache)
{
    printf("Testing %s (%p) '%s'\r\n", label, string, string);
    printf("Formats as: '");
    printf(string, 1, 2, 3);
    printf("'\r\n");
    uint32_t nullvalue = inner_string_test(string, test_noop, "null op", 0, evict_cache);
    inner_string_test(string, test_memcpy_aligned, "memcpy - aligned len", nullvalue, evict_cache);
    inner_string_test(string, test_memcpy_unaligned, "memcpy - unaligned len", nullvalue, evict_cache);
    inner_string_test(string, test_memcpy_unaligned2, "memcpy - unaligned start&len", nullvalue, evict_cache);
    inner_string_test(string, test_strcpy, "strcpy", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy, "naive strcpy", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a0, "naive strcpy (a0)", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a2, "naive strcpy (a2)", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a3, "naive strcpy (a3)", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a4, "naive strcpy (a4)", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a5, "naive strcpy (a5)", nullvalue, evict_cache);
    inner_string_test(string, test_naive_strcpy_a6, "naive strcpy (a6)", nullvalue, evict_cache);
    inner_string_test(string, test_sprintf, "sprintf", nullvalue, evict_cache);
    inner_string_test(string, test_sprintf_arg, "sprintf format arg", nullvalue, evict_cache);
}

DEFINE_SOLO_TESTCASE(03_byte_load_test_strings)

/* Test various operations on strings in various regions */
static void a_03_byte_load_test_strings()
{
    string_test(dramtest, "DRAM", 0);
    string_test(iramtest, "IRAM", 0);
    string_test(iromtest, "Cached flash", 0);
    string_test(iromtest, "'Uncached' flash", 1);
    TEST_PASS();
}

static volatile bool frc1_ran;
static volatile bool frc1_finished;
static volatile char frc1_buf[80];

DEFINE_SOLO_TESTCASE(03_byte_load_test_isr)

static void frc1_interrupt_handler(void)
{
    frc1_ran = true;
    timer_set_run(FRC1, false);
    strcpy((char *)frc1_buf, iramtest);
    frc1_finished = true;
}

/* Verify that the unaligned loader can run inside an ISR */
static void a_03_byte_load_test_isr()
{
    printf("Testing behaviour inside ISRs...\r\n");
    timer_set_interrupts(FRC1, false);
    timer_set_run(FRC1, false);
    _xt_isr_attach(INUM_TIMER_FRC1, frc1_interrupt_handler, NULL);
    timer_set_frequency(FRC1, 1000);
    timer_set_interrupts(FRC1, true);
    timer_set_run(FRC1, true);
    sdk_os_delay_us(2000);

    if(!frc1_ran)
      TEST_FAIL_MESSAGE("ERROR: FRC1 timer exception never fired.\r\n");
    else if(!frc1_finished)
      TEST_FAIL_MESSAGE("ERROR: FRC1 timer exception never finished.\r\n");
    else if(strcmp((char *)frc1_buf, iramtest))
      TEST_FAIL_MESSAGE("ERROR: FRC1 strcpy from IRAM failed.\r\n");
    else
      TEST_PASS();
}

DEFINE_SOLO_TESTCASE(03_byte_load_test_sign_extension)

static void a_03_byte_load_test_sign_extension()
{
    /* this step seems to be necessary so the compiler will actually generate l16si */
    int16_t *shorts_p = (int16_t *)unsigned_shorts;
    if(shorts_p[0] == -3 && shorts_p[1] == -4 && shorts_p[2] == -5 && shorts_p[3] == -32767 && shorts_p[4] == 44)
    {
      TEST_PASS();
    } else {
      sprintf(buf, "l16si sign extension failed. Got values %d %d %d %d %d\r\n", shorts_p[0], shorts_p[1], shorts_p[2], shorts_p[3], shorts_p[4]);
      TEST_FAIL_MESSAGE(buf);
    }
}


/* test that running unaligned loads in a running FreeRTOS system doesn't break things

   The following tests run inside a FreeRTOS task, after everything else.
*/
DEFINE_SOLO_TESTCASE(03_byte_load_test_system_interaction);

static void task_load_test_system_interaction()
{
    uint32_t start = xTaskGetTickCount();
    printf("Starting system/timer interaction test (takes approx 1 second)...\n");
    for(int i = 0; i < 5000; i++) {
        test_naive_strcpy_a0(iromtest);
        test_naive_strcpy_a2(iromtest);
        test_naive_strcpy_a3(iromtest);
        test_naive_strcpy_a4(iromtest);
        test_naive_strcpy_a5(iromtest);
        test_naive_strcpy_a6(iromtest);
        /*
        const volatile char *string = iromtest;
        volatile char *to = dest;
        while((*to++ = *string++))
            ;
        */
    }
    uint32_t ticks = xTaskGetTickCount() - start;
    printf("Timer interaction test PASSED after %d ticks.\n", ticks);
    TEST_PASS();
}

static void a_03_byte_load_test_system_interaction()
{
    xTaskCreate(task_load_test_system_interaction, "interactionTask", 256, NULL, 2, NULL);
    while(1) {
      vTaskDelay(100);
    }
}

/* The following "sanity tests" are designed to try to execute every code path
 * of the LoadStoreError handler, with a variety of offsets and data values
 * designed to catch any mask/shift errors, sign-extension bugs, etc */
DEFINE_SOLO_TESTCASE(03_byte_load_test_sanity)

/* (Contrary to expectations, 'mov a15, a15' in Xtensa is not technically a
 * no-op, but is officially "undefined and reserved for future use", so we need
 * a special case in the case where reg == "a15" so we don't end up generating
 * those opcodes.  GCC is smart enough to optimize away the whole conditional
 * and just insert the correct asm block, since `reg` is a static argument.) */
#define LOAD_VIA_REG(op, reg, addr, var) \
    if (strcmp(reg, "a15")) { \
        asm volatile ( \
        "mov a15, " reg "\n\t" \
        op " " reg ", %1, 0\n\t" \
        "mov %0, " reg "\n\t" \
        "mov " reg ", a15\n\t" \
        : "=r" (var) : "r" (addr) : "a15" ); \
    } else { \
        asm volatile ( \
        op " " reg ", %1, 0\n\t" \
        "mov %0, " reg "\n\t" \
        : "=r" (var) : "r" (addr) : "a15" ); \
    }

#define TEST_LOAD(op, reg, addr, value) \
    { \
        int32_t result; \
        LOAD_VIA_REG(op, reg, addr, result); \
        if (result != value) sanity_test_failed(op, reg, addr, value, result); \
    }

static void sanity_test_failed(const char *testname, const char *reg, const void *addr, int32_t value, int32_t result) {
    uint32_t actual_data = *(uint32_t *)((uint32_t)addr & 0xfffffffc);
    sprintf(buf, "%s %s from %p (32-bit value: 0x%x): Expected 0x%08x (%d), got 0x%08x (%d)\n", testname, reg, addr, actual_data, value, value, result, result);
    TEST_FAIL_MESSAGE(buf);
}

static void sanity_test_l8ui(const void *addr, int32_t value) {
    TEST_LOAD("l8ui", "a0", addr, value);
    TEST_LOAD("l8ui", "a1", addr, value);
    TEST_LOAD("l8ui", "a2", addr, value);
    TEST_LOAD("l8ui", "a3", addr, value);
    TEST_LOAD("l8ui", "a4", addr, value);
    TEST_LOAD("l8ui", "a5", addr, value);
    TEST_LOAD("l8ui", "a6", addr, value);
    TEST_LOAD("l8ui", "a7", addr, value);
    TEST_LOAD("l8ui", "a8", addr, value);
    TEST_LOAD("l8ui", "a9", addr, value);
    TEST_LOAD("l8ui", "a10", addr, value);
    TEST_LOAD("l8ui", "a11", addr, value);
    TEST_LOAD("l8ui", "a12", addr, value);
    TEST_LOAD("l8ui", "a13", addr, value);
    TEST_LOAD("l8ui", "a14", addr, value);
    TEST_LOAD("l8ui", "a15", addr, value);
}

static void sanity_test_l16ui(const void *addr, int32_t value) {
    TEST_LOAD("l16ui", "a0", addr, value);
    TEST_LOAD("l16ui", "a1", addr, value);
    TEST_LOAD("l16ui", "a2", addr, value);
    TEST_LOAD("l16ui", "a3", addr, value);
    TEST_LOAD("l16ui", "a4", addr, value);
    TEST_LOAD("l16ui", "a5", addr, value);
    TEST_LOAD("l16ui", "a6", addr, value);
    TEST_LOAD("l16ui", "a7", addr, value);
    TEST_LOAD("l16ui", "a8", addr, value);
    TEST_LOAD("l16ui", "a9", addr, value);
    TEST_LOAD("l16ui", "a10", addr, value);
    TEST_LOAD("l16ui", "a11", addr, value);
    TEST_LOAD("l16ui", "a12", addr, value);
    TEST_LOAD("l16ui", "a13", addr, value);
    TEST_LOAD("l16ui", "a14", addr, value);
    TEST_LOAD("l16ui", "a15", addr, value);
}

static void sanity_test_l16si(const void *addr, int32_t value) {
    TEST_LOAD("l16si", "a0", addr, value);
    TEST_LOAD("l16si", "a1", addr, value);
    TEST_LOAD("l16si", "a2", addr, value);
    TEST_LOAD("l16si", "a3", addr, value);
    TEST_LOAD("l16si", "a4", addr, value);
    TEST_LOAD("l16si", "a5", addr, value);
    TEST_LOAD("l16si", "a6", addr, value);
    TEST_LOAD("l16si", "a7", addr, value);
    TEST_LOAD("l16si", "a8", addr, value);
    TEST_LOAD("l16si", "a9", addr, value);
    TEST_LOAD("l16si", "a10", addr, value);
    TEST_LOAD("l16si", "a11", addr, value);
    TEST_LOAD("l16si", "a12", addr, value);
    TEST_LOAD("l16si", "a13", addr, value);
    TEST_LOAD("l16si", "a14", addr, value);
    TEST_LOAD("l16si", "a15", addr, value);
}

static void a_03_byte_load_test_sanity(void) {
    printf("== Performing sanity tests (sanity_test_data @ %p)...\n", sanity_test_data);

    sanity_test_l8ui(sanity_test_data + 0, 0x01);
    sanity_test_l8ui(sanity_test_data + 1, 0x55);
    sanity_test_l8ui(sanity_test_data + 2, 0x7e);
    sanity_test_l8ui(sanity_test_data + 3, 0x2a);
    sanity_test_l8ui(sanity_test_data + 4, 0x81);
    sanity_test_l8ui(sanity_test_data + 5, 0xd5);
    sanity_test_l8ui(sanity_test_data + 6, 0xfe);
    sanity_test_l8ui(sanity_test_data + 7, 0xaa);

    sanity_test_l16ui(sanity_test_data + 0, 0x5501);
    sanity_test_l16ui(sanity_test_data + 2, 0x2a7e);
    sanity_test_l16ui(sanity_test_data + 4, 0xd581);
    sanity_test_l16ui(sanity_test_data + 6, 0xaafe);

    sanity_test_l16si(sanity_test_data + 0, 0x5501);
    sanity_test_l16si(sanity_test_data + 2, 0x2a7e);
    sanity_test_l16si(sanity_test_data + 4, -10879);
    sanity_test_l16si(sanity_test_data + 6, -21762);

    printf("== Sanity tests completed.\n");
    TEST_PASS();
}
