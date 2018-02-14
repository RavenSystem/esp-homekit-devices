#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "lwip.h"

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/ssl.h>
#include <wolfcrypt/test/test.h>
#include <wolfcrypt/benchmark/benchmark.h>


/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/* UART definitions */
extern UART_HandleTypeDef huart4;


/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
typedef struct func_args {
    int    argc;
    char** argv;
    int    return_code;
} func_args;

const char menu1[] = "\r\n"
    "\tt. WolfSSL Test\r\n"
    "\tb. WolfSSL Benchmark\r\n";

/*****************************************************************************
 * Private functions
 ****************************************************************************/


/*****************************************************************************
 * Public functions
 ****************************************************************************/
void wolfCryptDemo(void const * argument)
{
    uint8_t buffer[1] = {'t'};
    func_args args;

    /* init code for LWIP */
    MX_LWIP_Init();

    while (1) {
        printf("\r\n\t\t\t\tMENU\r\n");
        printf(menu1);
        printf("Please select one of the above options: ");

        HAL_UART_Receive(&huart4, buffer, sizeof(buffer), 1000);

        switch (buffer[0]) {

        case 't':
            memset(&args, 0, sizeof(args));
            printf("\nCrypt Test\n");
            wolfcrypt_test(&args);
            printf("Crypt Test: Return code %d\n", args.return_code);
            break;

        case 'b':
            memset(&args, 0, sizeof(args));
            printf("\nBenchmark Test\n");
            benchmark_test(&args);
            printf("Benchmark Test: Return code %d\n", args.return_code);
            break;

        // All other cases go here
        default: printf("\r\nSelection out of range\r\n"); break;
        }
    }
}

extern RTC_HandleTypeDef hrtc;
double current_time()
{
	RTC_TimeTypeDef time;
	RTC_DateTypeDef date;
	uint32_t subsec;

	/* must get time and date here due to STM32 HW bug */
	HAL_RTC_GetTime(&hrtc, &time, FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &date, FORMAT_BIN);
	subsec = (255 - time.SubSeconds) * 1000 / 255;

	(void)date;

	/* return seconds.milliseconds */
	return ((double)time.Hours * 24) +
		   ((double)time.Minutes * 60) +
			(double)time.Seconds +
		   ((double)subsec/1000);
}
