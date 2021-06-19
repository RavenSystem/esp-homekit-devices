/* Example SPI transfert
 *
 * This sample code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include <stdio.h>
#include "esp/spi.h"

/*
 * Possible Data Structure of SPI Transaction
 *
 * [COMMAND]+[ADDRESS]+[DataOUT]+[DUMMYBITS]+[DataIN]
 *
 * [COMMAND]+[ADDRESS]+[DUMMYBITS]+[DataOUT]
 *
 */


void loop(void *pvParameters)
{
    uint32_t time = 0 ; // SPI transmission time
    float avr_time = 0 ; // Average of SPI transmission
    float u = 0 ;

    spi_init(1, SPI_MODE0, SPI_FREQ_DIV_1M, 1, SPI_LITTLE_ENDIAN, false); // init SPI module


    while(1) {

        time = sdk_system_get_time();

        spi_set_command(1,1,1) ; // Set one command bit to 1
        spi_set_address(1,4,8) ; // Set 4 address bits to 8
        spi_set_dummy_bits(1,4,false); // Set 4 dummy bit before Dout

        spi_repeat_send_16(1,0xC584,10);  // Send 1 bit command + 4 bits address + 4 bits dummy + 160 bits data

        spi_clear_address(1); // remove address
        spi_clear_command(1); // remove command
        spi_clear_dummy(1); // remove dummy


        time = sdk_system_get_time() -time ;
        avr_time = ((avr_time * (float)u ) + (float)time)/((float)u+1.0)  ; // compute average
        u++;
        if (u==100) {
            u=0 ;
            printf("Time: %f\n",avr_time);
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
}


void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());
    xTaskCreate(loop, "loop", 1024, NULL, 2, NULL);
}
