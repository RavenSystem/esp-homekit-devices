#include <stdio.h>
#include <sys/time.h>

#include "fs_test.h"

// get current time POSIX
// http://stackoverflow.com/questions/3756323/getting-the-current-time-in-milliseconds
static fs_time_t get_current_time(void)
{
    struct timeval tv; 
    fs_time_t ms;

    gettimeofday(&tv, NULL);

    ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return ms; 
}

int main(int argc, char *argv[])
{
    int result;
    printf("Start testing\n");

    if (fs_load_test_run(100000)) {
        printf("PASS\n");
        result = 0;
    } else {
        printf("FAIL\n");
        result = -1;
    }

    float read_rate, write_rate;
    if (fs_speed_test_run(get_current_time, &write_rate, &read_rate)) {
        printf("Read speed: %.1f bytes/ms\n", read_rate); 
        printf("Write speed: %.1f bytes/ms\n", write_rate); 
    } else {
        printf("FAIL\n");
        result -= 1;
    }

    printf("Testing finished\n");
    return result;
}
