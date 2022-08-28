/*
 * Example code to test crc and speed
 */
/////////////////////////////////Lib
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include <esp/hwrand.h>

//crc lib
#include "crc_generic.h"

#define NUMBER_COMPUTE_TEST 1000

unsigned char check_data[] = { "123456789" };
uint8_t tab_data[512];

void crc_8bit(void *pvParameters) {

    config_crc_8 customcrc ; // my crc object
    crc_8 tabsrc[256]; // my crc look-up table

    //init crc parameters  (MAXIM parameters)
    crc_8_generic_init(&customcrc,0x31, 8, 0x00, 0x00, 1, 1, 1);

    //generate table
    crc_8_generate_table(&customcrc, tabsrc, sizeof(tabsrc));

    //show setting of crc
    printf("\nCRC library v1.0 written on 11/02/2017 by Zaltora\n");
    printf("-------------------------------------------------\n");
    printf("\n");
    printf("Parameters:\n");
    printf("\n");
    printf(" polynom     :  0x%02X\n", customcrc.polynom);
    printf(" order       :  %d\n", customcrc.order);
    printf(" crcinit     :  0x%02X direct, 0x%x nondirect\n", customcrc.private.crcinit_direct, customcrc.private.crcinit_nondirect);
    printf(" crcxor      :  0x%02X\n", customcrc.crcxor);
    printf(" refin       :  %d\n", customcrc.refin);
    printf(" refout      :  %d\n", customcrc.refout);
    printf("\n");
    printf("check_data   :  '%s' (%d bytes)\n", check_data, sizeof(check_data));
    printf("\n");

    //show table
    printf("Lookup table generated:\n");
    printf("\n");
    printf("tabsrc[256]  = {");
    for (uint16_t i = 0 ; i < 256 ; i++)
    {
        if(!(i%8)) printf("\n");
        printf("0x%02X, ",tabsrc[i]);
    }
    printf("\n};\n\n");

    printf("Check value results with all algorithms:\n");
    printf("\n");

    //try different crc algorithm
    crc_8_generic_select_algo(&customcrc, tabsrc, sizeof(tabsrc), CRC_TABLE);
    printf("CRC_TABLE\t\t: 0x%02X\n", crc_8_generic_compute(&customcrc, check_data, sizeof(check_data)));
    crc_8_generic_select_algo(&customcrc, tabsrc,  sizeof(tabsrc), CRC_TABLE_FAST);
    printf("CRC_TABLE_FAST\t\t: 0x%02X\n", crc_8_generic_compute(&customcrc, check_data, sizeof(check_data)));
    crc_8_generic_select_algo(&customcrc, NULL, 0, CRC_BIT_TO_BIT);
    printf("CRC_BIT_TO_BIT\t\t: 0x%02X\n", crc_8_generic_compute(&customcrc, check_data, sizeof(check_data)));
    crc_8_generic_select_algo(&customcrc, NULL, 0, CRC_BIT_TO_BIT_FAST);
    printf("CRC_BIT_TO_BIT_FAST\t: 0x%02X\n", crc_8_generic_compute(&customcrc, check_data, sizeof(check_data)));
    crc_8_generic_select_algo(&customcrc, crc_8_tab_MAXIM, sizeof(crc_8_tab_MAXIM), CRC_TABLE_FAST);
    printf("CRC_TABLE_BUILTIN\t: 0x%02X\n\n", crc_8_generic_compute(&customcrc, check_data, sizeof(check_data)));

    printf("Test speed algorithms with random data:\n");
    printf("\n");

    //data to process
    printf("%u bytes of DATA:",sizeof(tab_data));
    for (uint16_t i = 0 ; i < sizeof(tab_data) ; i++)
    {
        tab_data[i] = (uint8_t)hwrand();
        if(!(i%32)) printf("\n");
        printf("%02X",tab_data[i]);
    }
    printf("\n\n");

    const uint32_t cst = NUMBER_COMPUTE_TEST ;
    char algo_txt[30] ;
    uint32_t time = 0;
    uint8_t select = 0;
    uint8_t result = 0;
    for(select = 0; select < 5 ; select++)
    {
        switch(select){
        case 0:
            crc_8_generic_select_algo(&customcrc, NULL, 0, CRC_BIT_TO_BIT);
            sprintf(algo_txt,"CRC_BIT_TO_BIT");
            break;
        case 1:
            crc_8_generic_select_algo(&customcrc, NULL, 0, CRC_BIT_TO_BIT_FAST);
            sprintf(algo_txt,"CRC_BIT_TO_BIT_FAST");
            break;
        case 2:
            crc_8_generic_select_algo(&customcrc, tabsrc, sizeof(tabsrc), CRC_TABLE);
            sprintf(algo_txt,"CRC_TABLE");
            break;
        case 3:
            crc_8_generic_select_algo(&customcrc, tabsrc, sizeof(tabsrc), CRC_TABLE_FAST);
            sprintf(algo_txt,"CRC_TABLE_FAST");
            break;
        case 4:
            crc_8_generic_select_algo(&customcrc, crc_8_tab_MAXIM, sizeof(crc_8_tab_MAXIM), CRC_TABLE_FAST);
            sprintf(algo_txt,"CRC_TABLE_FAST_BUILTIN");
            break;
        }
        printf("test speed algorithm %s \n",algo_txt);
        time = sdk_system_get_time();
        for (uint32_t i = 0 ; i < cst ; i++)
        {
            result = crc_8_generic_compute(&customcrc, tab_data, sizeof(tab_data));
        }
        time = sdk_system_get_time()-time ;
        printf("Speed algorithm: %.3f us\n",(float)time/(float)cst);
        printf("Result algorithm: %02X\n\n",result);
    }
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS) ;
    }
}

void  user_init(void)
{
	sdk_system_update_cpu_freq(160);
	uart_set_baud(0, 115200);
	printf("SDK version:%s\n", sdk_system_get_sdk_version());
	printf("Start\n\n");
	vTaskDelay(2000 / portTICK_PERIOD_MS) ;
	xTaskCreate(crc_8bit, "crc_8bit", 512, NULL, 2, NULL);
}
