#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "etstimer.h"

#include "testcase.h"

DEFINE_SOLO_TESTCASE(06_ets_timers);

typedef struct {
    ETSTimer handle;
    uint32_t start_time;
    uint32_t fire_count;
} test_timer_t;

#define TEST_TIMERS_NUMBER 2
static test_timer_t timers[TEST_TIMERS_NUMBER];

static uint32_t get_current_time()
{
     return timer_get_count(FRC2) / 5000;  // to get roughly 1ms resolution
}

static void timer_0_cb(void *arg)
{
    uint32_t v = (uint32_t)arg;
    uint32_t delay = get_current_time() - timers[0].start_time;
    timers[0].fire_count++;

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xAA, v, "Timer 0 argument invalid");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, timers[0].fire_count, "Timer 0 repeat error");

    printf("Timer 0 delay: %d\n", delay);
    // Timer should fire in 100ms
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 100, delay, "Timer 0 time wrong");
}

static void timer_1_cb(void *arg)
{
    uint32_t v = (uint32_t)arg;
    uint32_t delay = get_current_time() - timers[1].start_time;

    timers[1].start_time = get_current_time();
    timers[1].fire_count++;

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xBB, v, "Timer 1 argument invalid");
    TEST_ASSERT_TRUE_MESSAGE(timers[1].fire_count < 6,
            "Timer 1 repeats after disarming");

    printf("Timer 1 delay: %d\n", delay);
    // Timer should fire in 100ms
    TEST_ASSERT_INT_WITHIN_MESSAGE(5, 50, delay, "Timer 1 time wrong");

    if (timers[1].fire_count == 5) {
        sdk_ets_timer_disarm(&timers[1].handle);
    }
}

static void test_task(void *pvParameters)
{
    sdk_ets_timer_disarm(&timers[0].handle);
    sdk_ets_timer_setfn(&timers[0].handle, timer_0_cb, (void*)0xAA);
    timers[0].start_time = get_current_time();
    sdk_ets_timer_arm(&timers[0].handle, 100, false);

    sdk_ets_timer_disarm(&timers[1].handle);
    sdk_ets_timer_setfn(&timers[1].handle, timer_1_cb, (void*)0xBB);
    timers[1].start_time = get_current_time();
    sdk_ets_timer_arm(&timers[1].handle, 50, true);   // repeating timer

    vTaskDelay(500 / portTICK_PERIOD_MS);

    TEST_ASSERT_EQUAL_INT_MESSAGE(1, timers[0].fire_count,
            "Timer hasn't fired");

    TEST_ASSERT_EQUAL_INT_MESSAGE(5, timers[1].fire_count,
            "Timer fire count isn't correct");

    TEST_PASS();
}

static void a_06_ets_timers(void)
{
    xTaskCreate(test_task, "test_task", 256, NULL, 2, NULL);
}
