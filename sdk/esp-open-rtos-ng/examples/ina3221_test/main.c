/*
 * Example of using INA3221
 *
 * Part of esp-open-rtos
 * Copyright (C) 2016 Zaltora
 * MIT Licensed as described in the file LICENSE
 */

#include "espressif/esp_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include <esp/uart.h>
#include <stdbool.h>
#include "ina3221/ina3221.h"

#define I2C_BUS 0
#define PIN_SCL 5
#define PIN_SDA 2
#define ADDR INA3221_ADDR_0

#define WARNING_CHANNEL 1
#define WARNING_CURRENT (40.0)

//#define STRUCT_SETTING 0
#define MODE false  // true : continuous  measurements // false : trigger measurements

void ina_measure(void *pvParameters)
{
    uint32_t measure_number = 0;
    float bus_voltage;
    float shunt_voltage;
    float shunt_current;
    bool warning = false ;

    // Create ina3221 device
    ina3221_t dev = {
            .i2c_dev.bus = I2C_BUS,
            .i2c_dev.addr = ADDR,
            .shunt = { 100 ,100 ,100 },  // shunt values are 100 mOhm for each channel
            .mask.mask_register = INA3221_DEFAULT_MASK, // Init
            .config.config_register = INA3221_DEFAULT_CONFIG, // Init
    };

#ifndef STRUCT_SETTING
    if(ina3221_setting(&dev ,MODE, true, true)) //  mode selection ,  bus and shunt activated
        goto error_loop;
    if(ina3221_enableChannel(&dev , true, true, true)) // Enable all channels
        goto error_loop;
    if(ina3221_setAverage(&dev, INA3221_AVG_64)) // 64 samples average
        goto error_loop;
    if(ina3221_setBusConversionTime(&dev, INA3221_CT_2116)) // 2ms by channel
        goto error_loop;
    if(ina3221_setShuntConversionTime(&dev, INA3221_CT_2116)) // 2ms by channel
        goto error_loop;
#else
    dev.config.mode = MODE; // mode selection
    dev.config.esht = true; // shunt enable
    dev.config.ebus = true; // bus enable
    dev.config.ch1 = true; // channel 1 enable
    dev.config.ch2 = true; // channel 2 enable
    dev.config.ch3 = true; // channel 3 enable
    dev.config.avg = INA3221_AVG_64; // 64 samples average
    dev.config.vbus = INA3221_CT_2116; // 2ms by channel (bus)
    dev.config.vsht = INA3221_CT_2116; // 2ms by channel (shunt)
    if(ina3221_sync(&dev))
        goto error_loop;
#endif

    ina3221_setWarningAlert(&dev, WARNING_CHANNEL-1, WARNING_CURRENT); //Set security flag overcurrent

    while(1)
    {
        measure_number++;
#if !MODE
        if (ina3221_trigger(&dev)) // Start a measure
            goto error_loop;
        printf("trig done, wait: ");
        do
        {
            printf("X");
            if (ina3221_getStatus(&dev)) // get mask
                goto error_loop;
            vTaskDelay(20/portTICK_PERIOD_MS);
            if(dev.mask.wf&(1<<(3-WARNING_CHANNEL)))
                warning = true ;
        } while(!(dev.mask.cvrf)); // check if measure done
#else
        if (ina3221_getStatus(&dev)) // get mask
            goto error_loop;
        if(dev.mask.wf&(1<<(3-WARNING_CHANNEL)))
            warning = true ;
#endif
        for (uint8_t i = 0 ; i < INA3221_BUS_NUMBER ; i++)
        {
            if(ina3221_getBusVoltage(&dev, i, &bus_voltage)) // Get voltage in V
                goto error_loop;
            if(ina3221_getShuntValue(&dev, i, &shunt_voltage, &shunt_current)) // Get voltage in mV and currant in mA
                goto error_loop;

            printf("\nC%u:Measure number %u\n",i+1,measure_number);
            if (warning && (i+1) == WARNING_CHANNEL) printf("C%u:Warning Current > %.2f mA !!\n",i+1,WARNING_CURRENT);
            printf("C%u:Bus voltage: %.02f V\n",i+1,bus_voltage );
            printf("C%u:Shunt voltage: %.02f mV\n",i+1,shunt_voltage );
            printf("C%u:Shunt current: %.02f mA\n\n",i+1,shunt_current );

        }
        warning = false ;
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }

    error_loop:
    printf("%s: error while com with INA3221\n", __func__);
    for(;;)
    {
        vTaskDelay(2000/portTICK_PERIOD_MS);
        printf("%s: error loop\n", __FUNCTION__);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    i2c_init(I2C_BUS, PIN_SCL, PIN_SDA, I2C_FREQ_400K);

    xTaskCreate(ina_measure, "Measurements_task", 512, NULL, 2, NULL);
}
