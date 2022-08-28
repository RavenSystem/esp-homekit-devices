#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysparam.h>

#include <espressif/spi_flash.h>
#include "espressif/esp_common.h"
#include "esp/uart.h"

#define CMD_BUF_SIZE 5000

const int status_base = -6;
const char *status_messages[] = {
    "SYSPARAM_ERR_NOMEM",
    "SYSPARAM_ERR_CORRUPT",
    "SYSPARAM_ERR_IO",
    "SYSPARAM_ERR_FULL",
    "SYSPARAM_ERR_BADVALUE",
    "SYSPARAM_ERR_NOINIT",
    "SYSPARAM_OK",
    "SYSPARAM_NOTFOUND",
    "SYSPARAM_PARSEFAILED",
};

void usage(void) {
    printf(
        "Available commands:\n"
        "  <key>?          -- Query the value of <key>\n"
        "  <key>=<value>   -- Set <key> to text <value>\n"
        "  <key>:<hexdata> -- Set <key> to binary value represented as hex\n"
        "  dump            -- Show all currently set keys/values\n"
        "  compact         -- Compact the sysparam area\n"
        "  reformat        -- Reinitialize (clear) the sysparam area\n"
        "  echo-off        -- Disable input echo\n"
        "  echo-on         -- Enable input echo\n"
        "  help            -- Show this help screen\n"
        );
}

size_t tty_readline(char *buffer, size_t buf_size, bool echo) {
    size_t i = 0;
    int c;

    while (true) {
        c = getchar();
        if (c == '\r' || c == '\n') {
            if (echo) putchar('\n');
            break;
        } else if (c == '\b' || c == 0x7f) {
            if (i) {
                if (echo) {
                    printf("\b \b");
                    fflush(stdout);
                }
                i--;
            }
        } else if (c < 0x20) {
            /* Ignore other control characters */
        } else if (i >= buf_size - 1) {
            if (echo) {
                putchar('\a');
                fflush(stdout);
            }
        } else {
            buffer[i++] = c;
            if (echo) {
                putchar(c);
                fflush(stdout);
            }
        }
    }

    buffer[i] = 0;
    return i;
}

void print_text_value(char *key, char *value) {
    printf("  '%s' = '%s'\n", key, value);
}

void print_binary_value(char *key, uint8_t *value, size_t len) {
    size_t i;

    printf("  %s:", key);
    for (i = 0; i < len; i++) {
        if (!(i & 0x0f)) {
            printf("\n   ");
        }
        printf(" %02x", value[i]);
    }
    printf("\n");
}

sysparam_status_t dump_params(void) {
    sysparam_status_t status;
    sysparam_iter_t iter;

    status = sysparam_iter_start(&iter);
    if (status < 0) return status;
    while (true) {
        status = sysparam_iter_next(&iter);
        if (status != SYSPARAM_OK) break;
        if (!iter.binary) {
            print_text_value(iter.key, (char *)iter.value);
        } else {
            print_binary_value(iter.key, iter.value, iter.value_len);
        }
    }
    sysparam_iter_end(&iter);

    if (status == SYSPARAM_NOTFOUND) {
        // This is the normal status when we've reached the end of all entries.
        return SYSPARAM_OK;
    } else {
        // Something apparently went wrong
        return status;
    }
}

uint8_t *parse_hexdata(char *string, size_t *result_length) {
    size_t string_len = strlen(string);
    uint8_t *buf = malloc(string_len / 2);
    uint8_t c;
    int i, j;
    bool digit = false;

    j = 0;
    for (i = 0; string[i]; i++) {
        c = string[i];
        if (c >= 0x30 && c <= 0x39) {
            c &= 0x0f;
        } else if (c >= 0x41 && c <= 0x46) {
            c -= 0x37;
        } else if (c >= 0x61 && c <= 0x66) {
            c -= 0x57;
        } else if (c == ' ') {
            continue;
        } else {
            free(buf);
            return NULL;
        }
        if (!digit) {
            buf[j] = c << 4;
        } else {
            buf[j++] |= c;
        }
        digit = !digit;
    }
    if (digit) {
        free(buf);
        return NULL;
    }
    *result_length = j;
    return buf;
}

void sysparam_editor_task(void *pvParameters) {
    char *cmd_buffer = malloc(CMD_BUF_SIZE);
    sysparam_status_t status;
    char *value;
    uint8_t *bin_value;
    size_t len;
    uint8_t *data;
    uint32_t base_addr, num_sectors;
    bool echo = true;

    if (!cmd_buffer) {
        printf("ERROR: Cannot allocate command buffer!\n");
        return;
    }

    printf("\nWelcome to the system parameter editor!  Enter 'help' for more information.\n\n");

    status = sysparam_get_info(&base_addr, &num_sectors);
    if (status == SYSPARAM_OK) {
        printf("[current sysparam region is at 0x%08x (%d sectors)]\n", base_addr, num_sectors);
    } else {
        printf("[NOTE: No current sysparam region (initialization problem during boot?)]\n");
        // Default to the same place/size as the normal system initialization
        // stuff, so if the user uses this utility to reformat it, it will put
        // it somewhere the system will find it later
        num_sectors = DEFAULT_SYSPARAM_SECTORS;
        base_addr = sdk_flashchip.chip_size - (5 + num_sectors) * sdk_flashchip.sector_size;
    }
    while (true) {
        printf("==> ");
        fflush(stdout);
        len = tty_readline(cmd_buffer, CMD_BUF_SIZE, echo);
        status = 0;
        if (!len) continue;
        if (cmd_buffer[len - 1] == '?') {
            cmd_buffer[len - 1] = 0;
            printf("Querying '%s'...\n", cmd_buffer);
            status = sysparam_get_string(cmd_buffer, &value);
            if (status == SYSPARAM_OK) {
                print_text_value(cmd_buffer, value);
                free(value);
            } else if (status == SYSPARAM_PARSEFAILED) {
                // This means it's actually a binary value
                status = sysparam_get_data(cmd_buffer, &bin_value, &len, NULL);
                if (status == SYSPARAM_OK) {
                    print_binary_value(cmd_buffer, bin_value, len);
                    free(value);
                }
            }
        } else if ((value = strchr(cmd_buffer, '='))) {
            *value++ = 0;
            printf("Setting '%s' to '%s'...\n", cmd_buffer, value);
            status = sysparam_set_string(cmd_buffer, value);
        } else if ((value = strchr(cmd_buffer, ':'))) {
            *value++ = 0;
            data = parse_hexdata(value, &len);
            if (value) {
                printf("Setting '%s' to binary data...\n", cmd_buffer);
                status = sysparam_set_data(cmd_buffer, data, len, true);
                free(data);
            } else {
                printf("Error: Unable to parse hex data\n");
            }
        } else if (!strcmp(cmd_buffer, "dump")) {
            printf("Dumping all params:\n");
            status = dump_params();
        } else if (!strcmp(cmd_buffer, "compact")) {
            printf("Compacting...\n");
            status = sysparam_compact();
        } else if (!strcmp(cmd_buffer, "reformat")) {
            printf("Re-initializing region...\n");
            status = sysparam_create_area(base_addr, num_sectors, true);
            if (status == SYSPARAM_OK) {
                // We need to re-init after wiping out the region we've been
                // using.
                status = sysparam_init(base_addr, 0);
            }
        } else if (!strcmp(cmd_buffer, "echo-on")) {
            echo = true;
            printf("Echo on\n");
        } else if (!strcmp(cmd_buffer, "echo-off")) {
            echo = false;
            printf("Echo off\n");
        } else if (!strcmp(cmd_buffer, "help")) {
            usage();
        } else {
            printf("Unrecognized command.\n\n");
            usage();
        }

        if (status != SYSPARAM_OK) {
            printf("! Operation returned status: %d (%s)\n", status, status_messages[status - status_base]);
        }
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    sdk_wifi_set_opmode(NULL_MODE);

    xTaskCreate(sysparam_editor_task, "sysparam_editor_task", 512, NULL, 2, NULL);
}
