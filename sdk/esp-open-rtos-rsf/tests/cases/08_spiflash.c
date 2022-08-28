#include <string.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp/timer.h>
#include <FreeRTOS.h>
#include <task.h>
#include <esp8266.h>
#include <stdio.h>
#include <testcase.h>

#include <spiflash.h>

DEFINE_SOLO_TESTCASE(08_spiflash_unaligned)

/**
 * Test unaligned access to spi flash.
 */
static void a_08_spiflash_unaligned(void)
{
    const int test_addr = 0x100000 - (4096 * 8);
    const char test_str[] = "test_string";
    const int buf_size = 256;
    uint8_t buf[buf_size];

    TEST_ASSERT_TRUE(spiflash_erase_sector(test_addr));
    TEST_ASSERT_TRUE(spiflash_erase_sector(test_addr + 4096));

    TEST_ASSERT_TRUE(
            spiflash_write(test_addr, (uint8_t*)test_str, sizeof(test_str)));

    TEST_ASSERT_TRUE(spiflash_read(test_addr, buf, buf_size));

    TEST_ASSERT_EQUAL_STRING(test_str, buf);

    TEST_ASSERT_TRUE(
            spiflash_write(test_addr + 31, (uint8_t*)test_str, sizeof(test_str)));
    TEST_ASSERT_TRUE(spiflash_read(test_addr + 31, buf, buf_size));
    TEST_ASSERT_EQUAL_STRING(test_str, buf);

    TEST_ASSERT_TRUE(
            spiflash_write(test_addr + 101, (uint8_t*)test_str, sizeof(test_str)));
    TEST_ASSERT_TRUE(spiflash_read(test_addr + 101, buf + 1, buf_size - 1));
    TEST_ASSERT_EQUAL_STRING(test_str, buf + 1);

    TEST_ASSERT_TRUE(
            spiflash_write(test_addr + 201, (uint8_t*)test_str + 1, sizeof(test_str) - 1));
    TEST_ASSERT_TRUE(spiflash_read(test_addr + 201, buf + 1, buf_size - 1));
    TEST_ASSERT_EQUAL_STRING(test_str + 1, buf + 1);

    TEST_PASS();
}
