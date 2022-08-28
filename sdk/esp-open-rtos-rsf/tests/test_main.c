#include "testcase.h"
#include <stdlib.h>
#include <esp/uart.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <espressif/esp_common.h>

/* Convert requirement enum to a string we can print */
static const char *get_requirements_name(const testcase_type_t arg) {
    switch(arg) {
    case SOLO:
        return "SOLO";
    case DUAL:
        return "DUAL";
    case EYORE_TEST:
        return "EYORE_TEST";
    default:
        return "UNKNOWN";
    }
}

static testcase_t *testcases;
static uint32_t testcases_count;
static uint32_t testcases_alloc;

void testcase_register(const testcase_t *testcase)
{
    /* Grow the testcases buffer to fit the new test case,
       this buffer will be freed before the test runs.
     */
    if(testcases_count == testcases_alloc) {
        testcases_alloc += 1;
        testcases = realloc(testcases, testcases_alloc * sizeof(testcase_t));
        if(!testcases) {
            printf("Failed to reallocate test case register length %d\n",
                    testcases_alloc);
            testcases_count = 0;
            testcases_alloc = 0;
        }
    }
    memcpy(&testcases[testcases_count++], testcase, sizeof(testcase_t));
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    sdk_wifi_set_opmode(NULL_MODE);
    printf("esp-open-rtos test runner.\n");
    printf("%d test cases are defined:\n\n", testcases_count);
    for(int i = 0; i < testcases_count; i++) {
        printf("CASE %d = %s %s\n", i, testcases[i].name,
                get_requirements_name(testcases[i].type));
    }

    printf("Enter A or B then number of test case to run, ie A0.\n");
    int case_idx = -1;
    char type;
    do {
        printf("> ");
        uart_rxfifo_wait(0,1);
        type = uart_getc(0);
        if(type != 'a' && type != 'A' && type != 'b' && type != 'B') {
            printf("Type must be A or B.\n");
            continue;
        }

        char idx_buf[6];
        for(int c = 0; c < sizeof(idx_buf); c++) {
            uart_rxfifo_wait(0,1);
            idx_buf[c] = uart_getc(0);
            if(idx_buf[c] == ' ') { /* Ignore spaces */
                c--;
                continue;
            }
            if(idx_buf[c] == '\r' || idx_buf[c] == '\n') {
                idx_buf[c] = 0;
                case_idx = atoi(idx_buf);
                break;
            }
            else if(idx_buf[c] < '0' || idx_buf[c] > '9') {
                break;
            }
        }

        if(case_idx == -1) {
            printf("Invalid case index");
        }
        else if(case_idx < 0 || case_idx >= testcases_count) {
            printf("Test case index out of range.\n");
        }
        else if((type == 'b' || type =='B') && testcases[case_idx].type == SOLO) {
            printf("No ESP type B for 'SOLO' test cases.\n");
        } else {
            break;
        }
    } while(1);
    if(type =='a')
        type = 'A';
    else if (type == 'b')
        type = 'B';
    testcase_t testcase;
    memcpy(&testcase, &testcases[case_idx], sizeof(testcase_t));
    /* Free the register of test cases now we have the one we're running */
    free(testcases);
    testcases_alloc = 0;
    testcases_count = 0;

    printf("\nRunning test case %d (%s %s) as instance %c "
                "\nDefinition at %s:%d\n***\n", case_idx,
           testcase.name, get_requirements_name(testcase.type), type,
           testcase.file, testcase.line);

    Unity.CurrentTestName = testcase.name;
    Unity.TestFile = testcase.file;
    Unity.CurrentTestLineNumber = testcase.line;
    Unity.NumberOfTests = 1;
    if(type=='A')
        testcase.a_fn();
    else
        testcase.b_fn();
}
