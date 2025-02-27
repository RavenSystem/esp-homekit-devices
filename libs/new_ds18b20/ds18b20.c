
#include "math.h"

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PORT_ENTER_CRITICAL()       portMUX_TYPE *my_spinlock = malloc(sizeof(portMUX_TYPE)); portMUX_INITIALIZE(my_spinlock); taskENTER_CRITICAL(my_spinlock)
#define PORT_EXIT_CRITICAL()        taskEXIT_CRITICAL(my_spinlock); free(my_spinlock)

#else

#include "FreeRTOS.h"
#include "task.h"

#define PORT_ENTER_CRITICAL()       taskENTER_CRITICAL()
#define PORT_EXIT_CRITICAL()        taskEXIT_CRITICAL()

#endif

#include "ds18b20.h"

#define DS18B20_WRITE_SCRATCHPAD 0x4E
#define DS18B20_READ_SCRATCHPAD  0xBE
#define DS18B20_COPY_SCRATCHPAD  0x48
#define DS18B20_READ_EEPROM      0xB8
#define DS18B20_READ_PWRSUPPLY   0xB4
#define DS18B20_SEARCHROM        0xF0
#define DS18B20_SKIP_ROM         0xCC
#define DS18B20_READROM          0x33
#define DS18B20_MATCHROM         0x55
#define DS18B20_ALARMSEARCH      0xEC
#define DS18B20_CONVERT_T        0x44

#define os_sleep_ms(x) vTaskDelay(((x) + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS)

#define DS18B20_FAMILY_ID 0x28
#define DS18S20_FAMILY_ID 0x10

#ifdef DS18B20_DEBUG
#define debug(fmt, ...) printf("%s" fmt "\n", "DS18B20: ", ## __VA_ARGS__);
#else
#define debug(fmt, ...)
#endif

uint8_t ds18b20_read_all(uint8_t pin, int8_t pin_output, ds_sensor_t *result) {
    onewire_addr_t addr;
    onewire_search_t search;
    uint8_t sensor_id = 0;

    onewire_search_start(&search);

    while ((addr = onewire_search_next(&search, pin, pin_output)) != ONEWIRE_NONE) {
        uint8_t crc = onewire_crc8((uint8_t *)&addr, 7);
        if (crc != (addr >> 56)){
            debug("CRC check failed: %02X %02X\n", (unsigned)(addr >> 56), crc);
            return 0;
        }

        onewire_reset(pin, pin_output);
        onewire_select(pin, pin_output, addr);
        
        PORT_ENTER_CRITICAL();
        onewire_write(pin, pin_output, DS18B20_CONVERT_T);
        // For parasitic devices, power must be applied within 10us after issuing
        // the convert command.
        onewire_power(pin, pin_output);
        PORT_EXIT_CRITICAL();
        
        vTaskDelay(750 / portTICK_PERIOD_MS);

        onewire_reset(pin, pin_output);
        onewire_select(pin, pin_output, addr);
        onewire_write(pin, pin_output, DS18B20_READ_SCRATCHPAD);

        uint8_t get[10];

        for (unsigned int k = 0; k < 9; k++){
            get[k]=onewire_read(pin, pin_output);
        }

        //debug("\n ScratchPAD DATA = %X %X %X %X %X %X %X %X %X\n",get[8],get[7],get[6],get[5],get[4],get[3],get[2],get[1],get[0]);
        crc = onewire_crc8(get, 8);

        if (crc != get[8]){
            debug("CRC check failed: %02X %02X\n", get[8], crc);
            return 0;
        }

        uint8_t temp_msb = get[1]; // Sign byte + lsbit
        uint8_t temp_lsb = get[0]; // Temp data plus lsb
        uint16_t temp = temp_msb << 8 | temp_lsb;

        float temperature;

        temperature = (temp * 625.0)/10000;
        //debug("Got a DS18B20 Reading: %d.%02d\n", (int)temperature, (int)(temperature - (int)temperature) * 100);
        result[sensor_id].id = sensor_id;
        result[sensor_id].value = temperature;
        sensor_id++;
    }
    return sensor_id;
}

float ds18b20_read_single(uint8_t pin, int8_t pin_output) {

    onewire_reset(pin, pin_output);
    onewire_skip_rom(pin, pin_output);
    
    PORT_ENTER_CRITICAL();
    onewire_write(pin, pin_output, DS18B20_CONVERT_T);
    // For parasitic devices, power must be applied within 10us after issuing
    // the convert command.
    onewire_power(pin, pin_output);
    PORT_EXIT_CRITICAL();
    
    vTaskDelay(750 / portTICK_PERIOD_MS);

    onewire_reset(pin, pin_output);
    onewire_skip_rom(pin, pin_output);
    onewire_write(pin, pin_output, DS18B20_READ_SCRATCHPAD);

    uint8_t get[10];

    for (unsigned int k = 0; k < 9; k++){
        get[k] = onewire_read(pin, pin_output);
    }

    //debug("\n ScratchPAD DATA = %X %X %X %X %X %X %X %X %X\n",get[8],get[7],get[6],get[5],get[4],get[3],get[2],get[1],get[0]);
    uint8_t crc = onewire_crc8(get, 8);

    if (crc != get[8]){
        debug("CRC check failed: %02X %02X", get[8], crc);
        return 0;
    }

    uint8_t temp_msb = get[1]; // Sign byte + lsbit
    uint8_t temp_lsb = get[0]; // Temp data plus lsb

    uint16_t temp = temp_msb << 8 | temp_lsb;

    float temperature;

    temperature = (temp * 625.0)/10000;
    return temperature;
    //debug("Got a DS18B20 Reading: %d.%02d\n", (int)temperature, (int)(temperature - (int)temperature) * 100);
}

bool ds18b20_measure(uint8_t pin, int8_t pin_output, ds18b20_addr_t addr, bool wait) {
    if (!onewire_reset(pin, pin_output)) {
        return false;
    }
    if (addr == DS18B20_ANY) {
        onewire_skip_rom(pin, pin_output);
    } else {
        onewire_select(pin, pin_output, addr);
    }
    
    PORT_ENTER_CRITICAL();
    onewire_write(pin, pin_output, DS18B20_CONVERT_T);
    // For parasitic devices, power must be applied within 10us after issuing
    // the convert command.
    onewire_power(pin, pin_output);
    PORT_EXIT_CRITICAL();

    if (wait) {
        os_sleep_ms(750);
        onewire_depower(pin, pin_output);
    }

    return true;
}

bool ds18b20_read_scratchpad(uint8_t pin, int8_t pin_output, ds18b20_addr_t addr, uint8_t *buffer) {
    uint8_t crc;
    uint8_t expected_crc;

    if (!onewire_reset(pin, pin_output)) {
        return false;
    }
    if (addr == DS18B20_ANY) {
        onewire_skip_rom(pin, pin_output);
    } else {
        onewire_select(pin, pin_output, addr);
    }
    onewire_write(pin, pin_output, DS18B20_READ_SCRATCHPAD);

    for (int i = 0; i < 8; i++) {
        buffer[i] = onewire_read(pin, pin_output);
    }
    crc = onewire_read(pin, pin_output);

    expected_crc = onewire_crc8(buffer, 8);
    if (crc != expected_crc) {
        debug("CRC check failed reading scratchpad: %02x %02x %02x %02x %02x %02x %02x %02x : %02x (expected %02x)\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], crc, expected_crc);
        return false;
    }

    return true;
}

float ds18b20_read_temperature(uint8_t pin, int8_t pin_output, ds18b20_addr_t addr) {
    uint8_t scratchpad[8];
    int16_t temp;

    if (!ds18b20_read_scratchpad(pin, pin_output, addr, scratchpad)) {
        return NAN;
    }

    temp = scratchpad[1] << 8 | scratchpad[0];

    float res;
    if ((uint8_t)addr == DS18B20_FAMILY_ID) {
        res = ((float)temp * 625.0)/10000;
    }
    else {
        temp = ((temp & 0xfffe) << 3) + (16 - scratchpad[6]) - 4;
        res = ((float)temp * 625.0)/10000 - 0.25;
    }
    return res;
}

float ds18b20_measure_and_read(uint8_t pin, int8_t pin_output, ds18b20_addr_t addr) {
    if (!ds18b20_measure(pin, pin_output, addr, true)) {
        return NAN;
    }
    return ds18b20_read_temperature(pin, pin_output, addr);
}

bool ds18b20_measure_and_read_multi(uint8_t pin, int8_t pin_output, ds18b20_addr_t *addr_list, unsigned int addr_count, float *result_list) {
    if (!ds18b20_measure(pin, pin_output, DS18B20_ANY, true)) {
        for (unsigned int i = 0; i < addr_count; i++) {
            result_list[i] = NAN;
        }
        return false;
    }
    return ds18b20_read_temp_multi(pin, pin_output, addr_list, addr_count, result_list);
}

unsigned int ds18b20_scan_devices(uint8_t pin, int8_t pin_output, ds18b20_addr_t *addr_list, unsigned int addr_count) {
    onewire_search_t search;
    onewire_addr_t addr;
    unsigned int found = 0;

    onewire_search_start(&search);
    while ((addr = onewire_search_next(&search, pin, pin_output)) != ONEWIRE_NONE) {
        uint8_t family_id = (uint8_t)addr;
        if (family_id == DS18B20_FAMILY_ID || family_id == DS18S20_FAMILY_ID) {
            if (found < addr_count) {
                addr_list[found] = addr;
            }
            found++;
        }
    }
    return found;
}

bool ds18b20_read_temp_multi(uint8_t pin, int8_t pin_output, ds18b20_addr_t *addr_list, unsigned int addr_count, float *result_list) {
    bool result = true;

    for (unsigned int i = 0; i < addr_count; i++) {
        result_list[i] = ds18b20_read_temperature(pin, pin_output, addr_list[i]);
        if (isnan(result_list[i])) {
            result = false;
        }
    }
    return result;
}


