#include "testcase.h"
#include <FreeRTOS.h>
#include <task.h>
#include <esp/uart.h>

/* Basic test cases to validate the FreeRTOS scheduler works */

DEFINE_SOLO_TESTCASE(01_scheduler_basic)
DEFINE_SOLO_TESTCASE(01_scheduler_priorities)

void set_variable(void *pvParameters)
{
    bool *as_bool = (bool *)pvParameters;
    *as_bool = true;
    /* deliberately a busywait at the end, not vTaskSuspend, to test priorities */
    while(1) { }
}

/* Really simple - do created tasks run? */
static void a_01_scheduler_basic()
{
    volatile bool a = false, b = false, c = false;
    printf("top of scheduler...\n");
    uart_flush_txfifo(0);

    xTaskCreate(set_variable, "set_a", 128, (void *)&a, tskIDLE_PRIORITY, NULL);
    xTaskCreate(set_variable, "set_b", 128, (void *)&b, tskIDLE_PRIORITY, NULL);
    xTaskCreate(set_variable, "set_c", 128, (void *)&c, tskIDLE_PRIORITY, NULL);

    TEST_ASSERT_FALSE_MESSAGE(a, "task set_a shouldn't run yet");
    TEST_ASSERT_FALSE_MESSAGE(b, "task set_b shouldn't run yet");
    TEST_ASSERT_FALSE_MESSAGE(c, "task set_c shouldn't run yet");

    vTaskDelay(5);

    TEST_ASSERT_TRUE_MESSAGE(a, "task set_a should have run");
    TEST_ASSERT_TRUE_MESSAGE(b, "task set_b should have run");
    TEST_ASSERT_TRUE_MESSAGE(c, "task set_c should have run");
    TEST_PASS();
}

/* Verify that a high-priority task will starve a lower priority task */
static void a_01_scheduler_priorities()
{
    /* Increase priority of the init task to make sure it always takes priority */
    vTaskPrioritySet(xTaskGetCurrentTaskHandle(), tskIDLE_PRIORITY+4);

    bool lower = false, higher = false;
    TaskHandle_t task_lower, task_higher;

    xTaskCreate(set_variable, "high_prio", 128, (void *)&higher, tskIDLE_PRIORITY+1, &task_higher);
    xTaskCreate(set_variable, "low_prio", 128, (void *)&lower, tskIDLE_PRIORITY, &task_lower);

    TEST_ASSERT_FALSE_MESSAGE(higher, "higher prio task should not have run yet");
    TEST_ASSERT_FALSE_MESSAGE(lower, "lower prio task should not have run yet");

    vTaskDelay(2);

    TEST_ASSERT_TRUE_MESSAGE(higher, "higher prio task should have run");
    TEST_ASSERT_FALSE_MESSAGE(lower, "lower prio task should not have run");

    /* Bump lower priority task over higher priority task */
    vTaskPrioritySet(task_lower, tskIDLE_PRIORITY+2);

    TEST_ASSERT_FALSE_MESSAGE(lower, "lower prio task should still not have run yet");

    vTaskDelay(1);

    TEST_ASSERT_TRUE_MESSAGE(lower, "lower prio task should have run");
    TEST_PASS();
}
