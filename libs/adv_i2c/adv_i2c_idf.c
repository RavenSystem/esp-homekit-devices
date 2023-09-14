/*
* Advanced I2C Driver for ESP-IDF
*
* Copyright 2023 José Antonio Jiménez Campos (@RavenSystem)
*
*/

#ifdef ESP_PLATFORM

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/semphr.h"

#include "adv_i2c.h"

int adv_i2c_init_hz(uint8_t bus, uint8_t scl_pin, uint8_t sda_pin, uint32_t freq, bool scl_pin_pullup, bool sda_pin_pullup) {
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
    
    esp_err_t res = i2c_param_config(bus, &i2c_config);
    
    if (res == ESP_OK) {
        res = i2c_driver_install(bus, i2c_config.mode, 0, 0, ESP_INTR_FLAG_IRAM);
    }
    
    return res;
}

static int private_adv_i2c_slave_read(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, uint8_t *buf, size_t buf_len, TickType_t xTicksToWait) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    if (data && data_len) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, slave_addr << 1, true);
        i2c_master_write(cmd, (void*) data, data_len, true);
    }
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (slave_addr << 1) | 1, true);
    i2c_master_read(cmd, buf, buf_len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t res = i2c_master_cmd_begin(bus, cmd, xTicksToWait);
    
    i2c_cmd_link_delete(cmd);
    
    if (res == ESP_ERR_TIMEOUT) {
        i2c_reset_tx_fifo(bus);
        i2c_reset_rx_fifo(bus);
    }
    
    return res;
}

static int private_adv_i2c_slave_write(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, const uint8_t *buf, size_t buf_len, TickType_t xTicksToWait) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, slave_addr << 1, true);
    
    if (data && data_len) {
        i2c_master_write(cmd, (void*) data, data_len, true);
    }
    
    i2c_master_write(cmd, (void*) buf, buf_len, true);
    i2c_master_stop(cmd);
    
    esp_err_t res = i2c_master_cmd_begin(bus, cmd, xTicksToWait);
    
    i2c_cmd_link_delete(cmd);
    
    if (res == ESP_ERR_TIMEOUT) {
        i2c_reset_tx_fifo(bus);
        i2c_reset_rx_fifo(bus);
    }
    
    return res;
}

int adv_i2c_slave_read(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, uint8_t *buf, size_t buf_len) {
    return private_adv_i2c_slave_read(bus, slave_addr, data, data_len, buf, buf_len, ADV_I2C_SEMAPHORE_TIMEOUT);
}

int adv_i2c_slave_read_no_wait(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, uint8_t *buf, size_t buf_len) {
    return private_adv_i2c_slave_read(bus, slave_addr, data, data_len, buf, buf_len, 0);
}

int adv_i2c_slave_write(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, const uint8_t *buf, size_t buf_len) {
    return private_adv_i2c_slave_write(bus, slave_addr, data, data_len, buf, buf_len, ADV_I2C_SEMAPHORE_TIMEOUT);
}

int adv_i2c_slave_write_no_wait(uint8_t bus, uint8_t slave_addr, const uint8_t *data, const size_t data_len, const uint8_t *buf, size_t buf_len) {
    return private_adv_i2c_slave_write(bus, slave_addr, data, data_len, buf, buf_len, 0);
}

#endif // ESP_PLATFORM
