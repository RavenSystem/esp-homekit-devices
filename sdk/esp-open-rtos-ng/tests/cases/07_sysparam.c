
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <sysparam.h>

#include <testcase.h>

// #define DEBUG

#ifdef DEBUG
#include <stdio.h>
#define debug(fmt, ...) printf("%s" fmt, "test: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

DEFINE_SOLO_TESTCASE(07_sysparam_basic_test);
DEFINE_SOLO_TESTCASE(07_sysparam_load_test);
DEFINE_SOLO_TESTCASE(07_sysparam_bool_test);

#define TEST_ITERATIONS         10
#define KEY_BUF_SIZE            32
#define TEST_STRING_BUF_SIZE    64
#define NUMBER_OF_TEST_DATA     20

typedef struct {
    uint32_t start_key_index;
    uint32_t key_index;
} test_data_t;

typedef enum {
    VALUE_STRING = 0,
    VALUE_INT32,
    VALUE_INT8,
    VALUE_BOOL,
    VALUE_ENUM_END
} value_type_t;


static uint32_t get_current_time()
{
     return timer_get_count(FRC2) / 5000;  // to get roughly 1ms resolution
}

/**
 * Recreate sysparam area
 */
static inline void init_sysparam()
{
    sysparam_status_t status;
    uint32_t base_addr, num_sectors;

    status = sysparam_get_info(&base_addr, &num_sectors);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    status = sysparam_create_area(base_addr, num_sectors, /*force=*/true);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    status = sysparam_init(base_addr, 0);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    debug("sysparam initialized at addr=%x, sectors=%d\n",
            base_addr, num_sectors);
}

/**
 * Initialize test data with random seed.
 */
static void test_data_init(test_data_t *data)
{
    debug("test_data_init\n");
    data->start_key_index = data->key_index = rand() % 100;
}

/**
 * Reset test data to the initial seed.
 */
static void test_data_reset(test_data_t *data)
{
    debug("test_data_reset\n");
    data->key_index = data->start_key_index;
}

/**
 * Get key string for the current data.
 */
static void test_data_get_key(test_data_t *data, char *key_buf)
{
    sprintf(key_buf, "key_%d", data->key_index);
    debug("test_data_get_key: key=%s\n", key_buf);
}

/**
 * Generate test string for the current data.
 */
static void test_data_get_string(test_data_t *data, char *str_buf)
{
    srand(data->key_index);
    for (int i = 0; i < TEST_STRING_BUF_SIZE - 1; ++i) {
        str_buf[i] = '0' + rand() % 74; // generate a char 0-9,a-z,A-Z and other
    }
    str_buf[TEST_STRING_BUF_SIZE-1] = 0;  // terminate string with zero
    debug("test_data_get_string: str=%s\n", str_buf);
}

/**
 * Generate test int32 value for the current data.
 */
static int32_t test_data_get_int32(test_data_t *data)
{
    srand(data->key_index);
    int32_t v = rand();
    debug("test_data_get_int32: value=%d\n", v);
    return v;
}

/**
 * Generate test int8 value for the current data.
 */
int8_t test_data_get_int8(test_data_t *data)
{
    srand(data->key_index);
    int8_t v = rand() % 256;
    debug("test_data_get_int8: value=%d\n", v);
    return v;
}

/**
 * Generate test bool value for the current data.
 */
bool test_data_get_bool(test_data_t *data)
{
    srand(data->key_index);
    bool v = rand() % 2;
    debug("test_data_get_bool, value=%s\n", v ? "true" : "false");
    return v;
}

/**
 * Get type of the current data.
 */
value_type_t test_data_get_type(test_data_t *data)
{
    srand(data->key_index);
    value_type_t t = rand() % VALUE_ENUM_END;
    debug("test_data_get_type: type=%d\n", t);
    return t;
}

/**
 * Generate next data.
 */
void test_data_next(test_data_t *data)
{
    data->key_index++;
    debug("test_data_next: key_index=%d\n", data->key_index);
}

static void write_test_values(test_data_t *data)
{
    sysparam_status_t status = SYSPARAM_ERR_BADVALUE;
    char key_buf[KEY_BUF_SIZE];
    char str_buf[TEST_STRING_BUF_SIZE];

    for (int i = 0; i < NUMBER_OF_TEST_DATA; ++i) {
        test_data_get_key(data, key_buf);
        switch (test_data_get_type(data)) {
            case VALUE_STRING:
                test_data_get_string(data, str_buf);
                status = sysparam_set_string(key_buf, str_buf);
                break;
            case VALUE_INT32:
                status = sysparam_set_int32(key_buf, test_data_get_int32(data));
                break;
            case VALUE_INT8:
                status = sysparam_set_int8(key_buf, test_data_get_int8(data));
                break;
            case VALUE_BOOL:
                status = sysparam_set_bool(key_buf, test_data_get_bool(data));
                break;
            case VALUE_ENUM_END:
            default:
                break;
        }

        TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

        test_data_next(data);
    }
}

static void verify_test_values(test_data_t *data)
{
    sysparam_status_t status = SYSPARAM_ERR_BADVALUE;
    char key_buf[KEY_BUF_SIZE];
    char expected_str_buf[TEST_STRING_BUF_SIZE];
    char *actual_str;
    int32_t actual_int32;
    int8_t actual_int8;
    bool actual_bool;

    for (int i = 0; i < NUMBER_OF_TEST_DATA; ++i) {
        test_data_get_key(data, key_buf);
        switch (test_data_get_type(data)) {
            case VALUE_STRING:
                test_data_get_string(data, expected_str_buf);
                status = sysparam_get_string(key_buf, &actual_str);
                TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
                TEST_ASSERT_EQUAL_STRING(expected_str_buf, actual_str);
                free(actual_str);
                break;
            case VALUE_INT32:
                status = sysparam_get_int32(key_buf, &actual_int32);
                TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
                TEST_ASSERT_EQUAL_INT(test_data_get_int32(data), actual_int32);
                break;
            case VALUE_INT8:
                status = sysparam_get_int8(key_buf, &actual_int8);
                TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
                TEST_ASSERT_EQUAL_INT(test_data_get_int8(data), actual_int8);
                break;
            case VALUE_BOOL:
                status = sysparam_get_bool(key_buf, &actual_bool);
                TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
                TEST_ASSERT_TRUE(test_data_get_bool(data) == actual_bool);
                break;
            case VALUE_ENUM_END:
            default:
                break;
        }

        test_data_next(data);
    }
}

static void clear_test_values(test_data_t *data)
{
    char key_buf[KEY_BUF_SIZE];
    sysparam_status_t status = SYSPARAM_ERR_BADVALUE;

    for (int i = 0; i < NUMBER_OF_TEST_DATA; ++i) {
        test_data_get_key(data, key_buf);
        status = sysparam_set_data(key_buf, NULL, 0, false);
        TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

        test_data_next(data);
    }
}


static void a_07_sysparam_load_test(void)
{
    test_data_t test_data;
    init_sysparam();
    uint32_t start_time = get_current_time();
    uint32_t free_heap_at_start = xPortGetFreeHeapSize();

    for (int i = 0; i < TEST_ITERATIONS; ++i) {
        test_data_init(&test_data);
        write_test_values(&test_data);
        test_data_reset(&test_data);
        verify_test_values(&test_data);
        test_data_reset(&test_data);
        clear_test_values(&test_data);
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(free_heap_at_start, xPortGetFreeHeapSize(),
            "Free heap size is less than at test start. Possible memory leak.");

    printf("Test took %d ms\n", get_current_time() - start_time);

    TEST_PASS();
}

static void a_07_sysparam_basic_test(void)
{
    sysparam_status_t status;
    int32_t int32_val = 0;
    int8_t int8_val = 0;
    char *str;
    bool bool_val;

    init_sysparam();

    status = sysparam_set_int32("int_1", -123);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    status = sysparam_get_int32("int_1", &int32_val);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_EQUAL_INT(-123, int32_val);

    status = sysparam_set_int8("int_2", -34);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    status = sysparam_get_int8("int_2", &int8_val);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_EQUAL_INT(-34, int8_val);

    status = sysparam_set_string("str_1", "test string");
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    status = sysparam_get_string("str_1", &str);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_EQUAL_STRING("test string", str);
    free(str);

    status = sysparam_set_bool("bool_true", true);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    status = sysparam_get_bool("bool_true", &bool_val);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_TRUE(bool_val);

    status = sysparam_set_bool("bool_false", false);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    status = sysparam_get_bool("bool_false", &bool_val);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_FALSE(bool_val);

    TEST_PASS();
}

typedef struct {
    const char *key;
    const char *str;
    bool value;
} bool_test_data_t;

const static bool_test_data_t bool_data[] = {
    {"str_true", "true", true},
    {"str_True", "True", true},
    {"str_TRUE", "TRUE", true},
    {"str_t", "t", true},
    {"str_T", "T", true},
    {"str_y", "y", true},
    {"str_Y", "Y", true},
    {"str_yes", "yes", true},
    {"str_Yes", "Yes", true},
    {"str_YES", "YES", true},
    {"str_1", "1", true},

    {"str_false", "false", false},
    {"str_False", "False", false},
    {"str_FALSE", "FALSE", false},
    {"str_f", "f", false},
    {"str_F", "F", false},
    {"str_n", "n", false},
    {"str_N", "N", false},
    {"str_no", "no", false},
    {"str_No", "No", false},
    {"str_NO", "NO", false},
    {"str_0", "0", false},
};

static void a_07_sysparam_bool_test(void)
{
    sysparam_status_t status;
    bool value;

    init_sysparam();

    for (int i = 0; i < sizeof(bool_data) / sizeof(bool_data[0]); ++i) {
        status = sysparam_set_string(bool_data[i].key, bool_data[i].str);
        TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    }

    status = sysparam_set_int8("int8_0", 0);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    status = sysparam_set_int8("int8_1", 1);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    status = sysparam_set_int32("int32_0", 0);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    status = sysparam_set_int32("int32_1", 1);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);

    for (int i = 0; i < sizeof(bool_data) / sizeof(bool_data[0]); ++i) {
        debug("Getting bool key=%s\n", bool_data[i].key);
        status = sysparam_get_bool(bool_data[i].key, &value);
        TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
        TEST_ASSERT_TRUE(bool_data[i].value == value);
    }

    status = sysparam_get_bool("int8_0", &value);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_FALSE(value);

    status = sysparam_get_bool("int8_1", &value);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_TRUE(value);

    status = sysparam_get_bool("int32_0", &value);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_FALSE(value);

    status = sysparam_get_bool("int32_1", &value);
    TEST_ASSERT_EQUAL_INT(SYSPARAM_OK, status);
    TEST_ASSERT_TRUE(value);

    TEST_PASS();
}
