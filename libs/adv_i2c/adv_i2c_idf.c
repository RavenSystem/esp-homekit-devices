/*
* Advanced I2C Driver for ESP-IDF
*
* Copyright 2023 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifdef ESP_PLATFORM

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "adv_i2c.h"

#ifndef ADV_I2C_SEMAPHORE_TIMEOUT_MS
#define ADV_I2C_SEMAPHORE_TIMEOUT_MS                (1000)
#endif

static SemaphoreHandle_t adv_i2c_bus_lock[I2C_MAX_BUS];

int adv_i2c_init_hz(uint8_t bus, uint8_t scl_pin, uint8_t sda_pin, uint32_t freq, bool scl_pin_pullup, bool sda_pin_pullup) {
    adv_i2c_bus_lock[bus] = xSemaphoreCreateMutex();
    
    gpio_reset_pin(scl_pin);
    gpio_reset_pin(sda_pin);
    
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .sda_pullup_en = sda_pin_pullup,
        .scl_io_num = scl_pin,
        .scl_pullup_en = scl_pin_pullup,
        .master.clk_speed = freq,
        .clk_flags = 0,
    };
    
    esp_err_t res;
    
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
    // See https://github.com/espressif/esp-idf/issues/10163
    if ((res = i2c_driver_install(bus, i2c_config.mode, 0, 0, 0)) != ESP_OK) {
        return res;
    }
    if ((res = i2c_param_config(bus, &i2c_config)) != ESP_OK) {
        return res;
    }
#else
    if ((res = i2c_param_config(bus, &i2c_config)) != ESP_OK) {
        return res;
    }
    if ((res = i2c_driver_install(bus, i2c_config.mode, 0, 0, 0)) != ESP_OK) {
        return res;
    }
#endif
    
    return ESP_OK;
}

int adv_i2c_slave_read(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, uint8_t *buf, size_t len) {
    xSemaphoreTake(adv_i2c_bus_lock[bus], pdMS_TO_TICKS(ADV_I2C_SEMAPHORE_TIMEOUT_MS));

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    if (data && data_len) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, slave_addr << 1, true);
        i2c_master_write(cmd, (void*) data, data_len, true);
    }
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slave_addr << 1) | 1, true);
    i2c_master_read(cmd, buf, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(bus, cmd, pdMS_TO_TICKS(ADV_I2C_SEMAPHORE_TIMEOUT_MS));

    i2c_cmd_link_delete(cmd);

    xSemaphoreGive(adv_i2c_bus_lock[bus]);
    return res;
}

int adv_i2c_slave_write(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, const uint8_t *buf, size_t len) {
    xSemaphoreTake(adv_i2c_bus_lock[bus], pdMS_TO_TICKS(ADV_I2C_SEMAPHORE_TIMEOUT_MS));

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1, true);
    
    if (data && data_len) {
        i2c_master_write(cmd, (void*) data, data_len, true);
    }
    
    i2c_master_write(cmd, (void*) buf, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t res = i2c_master_cmd_begin(bus, cmd, pdMS_TO_TICKS(ADV_I2C_SEMAPHORE_TIMEOUT_MS));
    
    i2c_cmd_link_delete(cmd);

    xSemaphoreGive(adv_i2c_bus_lock[bus]);
    return res;
}


#endif // ESP_PLATFORM
