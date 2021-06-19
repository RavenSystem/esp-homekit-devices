/* Serial terminal example
 * UART RX is interrupt driven
 * Implements a simple GPIO terminal for setting and clearing GPIOs
 *
 * This sample code is in the public domain.
 */

#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#define MAX_ARGC (10)

static void cmd_on(uint32_t argc, char *argv[])
{
    if (argc >= 2) {
        for(int i=1; i<argc; i++) {
            uint8_t gpio_num = atoi(argv[i]);
            gpio_enable(gpio_num, GPIO_OUTPUT);
            gpio_write(gpio_num, true);
            printf("On %d\n", gpio_num);
        }
    } else {
        printf("Error: missing gpio number.\n");
    }
}

static void cmd_off(uint32_t argc, char *argv[])
{
    if (argc >= 2) {
        for(int i=1; i<argc; i++) {
            uint8_t gpio_num = atoi(argv[i]);
            gpio_enable(gpio_num, GPIO_OUTPUT);
            gpio_write(gpio_num, false);
            printf("Off %d\n", gpio_num);
        }
    } else {
        printf("Error: missing gpio number.\n");
    }
}

static void cmd_help(uint32_t argc, char *argv[])
{
    printf("on <gpio number> [ <gpio number>]+     Set gpio to 1\n");
    printf("off <gpio number> [ <gpio number>]+    Set gpio to 0\n");
    printf("sleep                                  Take a nap\n");
    printf("\nExample:\n");
    printf("  on 0<enter> switches on gpio 0\n");
    printf("  on 0 2 4<enter> switches on gpios 0, 2 and 4\n");
}

static void cmd_sleep(uint32_t argc, char *argv[])
{
    printf("Type away while I take a 2 second nap (ie. let you test the UART HW FIFO\n");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

static void handle_command(char *cmd)
{
    char *argv[MAX_ARGC];
    int argc = 1;
    char *temp, *rover;
    memset((void*) argv, 0, sizeof(argv));
    argv[0] = cmd;
    rover = cmd;
    // Split string "<command> <argument 1> <argument 2>  ...  <argument N>"
    // into argv, argc style
    while(argc < MAX_ARGC && (temp = strstr(rover, " "))) {
        rover = &(temp[1]);
        argv[argc++] = rover;
        *temp = 0;
    }

    if (strlen(argv[0]) > 0) {
        if (strcmp(argv[0], "help") == 0) cmd_help(argc, argv);
        else if (strcmp(argv[0], "on") == 0) cmd_on(argc, argv);
        else if (strcmp(argv[0], "off") == 0) cmd_off(argc, argv);
        else if (strcmp(argv[0], "sleep") == 0) cmd_sleep(argc, argv);
        else printf("Unknown command %s, try 'help'\n", argv[0]);
    }
}

static void gpiomon()
{
    char ch;
    char cmd[81];
    int i = 0;
    printf("\n\n\nWelcome to gpiomon. Type 'help<enter>' for, well, help\n");
    printf("%% ");
    fflush(stdout); // stdout is line buffered
    while(1) {
        if (read(0, (void*)&ch, 1)) { // 0 is stdin
            printf("%c", ch);
            fflush(stdout);
            if (ch == '\n' || ch == '\r') {
                cmd[i] = 0;
                i = 0;
                printf("\n");
                handle_command((char*) cmd);
                printf("%% ");
                fflush(stdout);
            } else {
                if (i < sizeof(cmd)) cmd[i++] = ch;
            }
        } else {
            printf("You will never see this print as read(...) is blocking\n");
        }
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    gpiomon();
}
