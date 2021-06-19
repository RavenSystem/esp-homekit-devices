#ifndef __FS_TEST_H__
#define __FS_TEST_H__

#include <stdint.h>
#include <stdbool.h>

bool fs_load_test_run(int32_t iterations);

typedef uint32_t fs_time_t;
typedef fs_time_t (*get_current_time_func)(void);

bool fs_speed_test_run(get_current_time_func get_time,
        float *write_rate,
        float *read_rate);

#endif  // __FS_TEST_H__
