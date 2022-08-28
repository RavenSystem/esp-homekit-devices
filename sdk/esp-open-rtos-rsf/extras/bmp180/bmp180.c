#include "bmp180.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"

#define BMP180_RX_QUEUE_SIZE      10
#define BMP180_TASK_PRIORITY      9

#define BMP180_VERSION_REG        0xD0
#define BMP180_CONTROL_REG        0xF4
#define BMP180_RESET_REG          0xE0
#define BMP180_OUT_MSB_REG        0xF6
#define BMP180_OUT_LSB_REG        0xF7
#define BMP180_OUT_XLSB_REG       0xF8

#define BMP180_CALIBRATION_REG    0xAA

//
// Values for BMP180_CONTROL_REG
//
#define BMP180_MEASURE_TEMP       0x2E
#define BMP180_MEASURE_PRESS      0x34

//
// CHIP ID stored in BMP180_VERSION_REG
//
#define BMP180_CHIP_ID            0x55

//
// Reset value for BMP180_RESET_REG
//
#define BMP180_RESET_VALUE        0xB6

static int bmp180_readRegister16(i2c_dev_t *dev, uint8_t reg, int16_t *r)
{
    uint8_t d[] = { 0, 0 };
    int error;

    if ((error = i2c_slave_read(dev->bus, dev->addr, &reg, d, 2)))
        return error;

    *r = ((int16_t)d[0] << 8) | (d[1]);
    return 0;
}

static int bmp180_start_Messurement(i2c_dev_t *dev, uint8_t cmd)
{
    uint8_t reg = BMP180_CONTROL_REG;

    return i2c_slave_write(dev->bus, dev->addr, &reg, &cmd, 1);
}

static bool bmp180_get_uncompensated_temperature(i2c_dev_t *dev, int32_t *ut)
{
    // Write Start Code into reg 0xF4.
    if (bmp180_start_Messurement(dev, BMP180_MEASURE_TEMP))
        return false;

    // Wait 5ms, datasheet states 4.5ms
    sdk_os_delay_us(5000);

    int16_t v;
    if (bmp180_readRegister16(dev, BMP180_OUT_MSB_REG, &v))
        return false;

    *ut = v;
    return true;
}

static bool bmp180_get_uncompensated_pressure(i2c_dev_t *dev, uint8_t oss, uint32_t *up)
{
    uint16_t us;

    // Limit oss and set the measurement wait time. The datasheet
    // states 4.5, 7.5, 13.5, 25.5ms for oss 0 to 3.
    switch (oss) {
      case 0: us = 5000; break;
      case 1: us = 8000; break;
      case 2: us = 14000; break;
      default: oss = 3; us = 26000; break;
    }

    // Write Start Code into reg 0xF4
    if (bmp180_start_Messurement(dev, BMP180_MEASURE_PRESS | (oss << 6)))
        return false;

    sdk_os_delay_us(us);

    uint8_t d[] = { 0, 0, 0 };
    uint8_t reg = BMP180_OUT_MSB_REG;
    if (i2c_slave_read(dev->bus, dev->addr, &reg, d, 3))
        return false;

    uint32_t r = ((uint32_t)d[0] << 16) | ((uint32_t)d[1] << 8) | d[2];
    r >>= 8 - oss;
    *up = r;
    return true;
}

// Returns true of success else false.
bool bmp180_fillInternalConstants(i2c_dev_t *dev, bmp180_constants_t *c)
{
    if (bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+0, &c->AC1) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+2, &c->AC2) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+4, &c->AC3) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+6, (int16_t *)&c->AC4) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+8, (int16_t *)&c->AC5) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+10, (int16_t *)&c->AC6) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+12, &c->B1) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+14, &c->B2) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+16, &c->MB) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+18, &c->MC) ||
        bmp180_readRegister16(dev, BMP180_CALIBRATION_REG+20, &c->MD)) {
        return false;
    }

#ifdef BMP180_DEBUG
    printf("%s: AC1:=%d AC2:=%d AC3:=%d AC4:=%u AC5:=%u AC6:=%u \n", __FUNCTION__, c->AC1, c->AC2, c->AC3, c->AC4, c->AC5, c->AC6);
    printf("%s: B1:=%d B2:=%d\n", __FUNCTION__, c->B1, c->B2);
    printf("%s: MB:=%d MC:=%d MD:=%d\n", __FUNCTION__, c->MB, c->MC, c->MD);
#endif

    // Error if any read as 0x0000 or 0xffff.
    return !(c->AC1 == 0x0000 || c->AC2 == 0x0000 || c->AC3 == 0x0000 ||
             c->AC4 == 0x0000 || c->AC5 == 0x0000 || c->AC6 == 0x0000 ||
             c->B1 == 0x0000 || c->B2 == 0x0000 ||
             c->MB == 0x0000 || c->MC == 0x0000 || c->MD == 0x0000 ||
             c->AC1 == 0xffff || c->AC2 == 0xffff || c->AC3 == 0xffff ||
             c->AC4 == 0xffff || c->AC5 == 0xffff || c->AC6 == 0xffff ||
             c->B1 == 0xffff || c->B2 == 0xffff ||
             c->MB == 0xffff || c->MC == 0xffff || c->MD == 0xffff);
}

bool bmp180_is_available(i2c_dev_t *dev)
{
    uint8_t id;
    uint8_t reg = BMP180_VERSION_REG;
    if (i2c_slave_read(dev->bus, dev->addr, &reg, &id, 1))
        return false;
    return id == BMP180_CHIP_ID;
}

bool bmp180_measure(i2c_dev_t *dev, bmp180_constants_t *c, int32_t *temperature,
                    uint32_t *pressure, uint8_t oss)
{
    int32_t T, P;

    if (!temperature && !pressure)
        return false;

    // Temperature is always needed, also required for pressure only.
    //
    // Calculation taken from BMP180 Datasheet
    int32_t UT, X1, X2, B5;
    if (!bmp180_get_uncompensated_temperature(dev, &UT))
        return false;

    X1 = ((UT - (int32_t)c->AC6) * (int32_t)c->AC5) >> 15;
    X2 = ((int32_t)c->MC << 11) / (X1 + (int32_t)c->MD);
    B5 = X1 + X2;
    T = (B5 + 8) >> 4;
    if (temperature)
        *temperature = T;
#ifdef BMP180_DEBUG
    printf("%s: T:= %ld.%d\n", __FUNCTION__, T/10, abs(T%10));
#endif

    if (pressure) {
        int32_t X3, B3, B6;
        uint32_t B4, B7, UP;

        if (!bmp180_get_uncompensated_pressure(dev, oss, &UP))
            return false;

        // Calculation taken from BMP180 Datasheet
        B6 = B5 - 4000;
        X1 = ((int32_t)c->B2 * ((B6 * B6) >> 12)) >> 11;
        X2 = ((int32_t)c->AC2 * B6) >> 11;
        X3 = X1 + X2;

        B3 = ((((int32_t)c->AC1 * 4 + X3) << oss) + 2) >> 2;
        X1 = ((int32_t)c->AC3 * B6) >> 13;
        X2 = ((int32_t)c->B1 * ((B6 * B6) >> 12)) >> 16;
        X3 = ((X1 + X2) + 2) >> 2;
        B4 = ((uint32_t)c->AC4 * (uint32_t)(X3 + 32768)) >> 15;
        B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> oss);

        if (B7 < 0x80000000UL) {
            P = (B7 * 2) / B4;
        } else {
            P = (B7 / B4) * 2;
        }

        X1 = (P >> 8) * (P >> 8);
        X1 = (X1 * 3038) >> 16;
        X2 = (-7357 * P) >> 16;
        P = P + ((X1 + X2 + (int32_t)3791) >> 4);
        if (pressure)
            *pressure = P;
#ifdef BMP180_DEBUG
        printf("%s: P:= %ld\n", __FUNCTION__, P);
#endif
    }
    return true;
}

// BMP180_Event_Command
typedef struct
{
    uint8_t cmd;
    const QueueHandle_t* resultQueue;
} bmp180_command_t;

// Just works due to the fact that QueueHandle_t is a "void *"
static QueueHandle_t bmp180_rx_queue[I2C_MAX_BUS] =  { NULL };
static TaskHandle_t bmp180_task_handle[I2C_MAX_BUS] = { NULL };

//
// Forward declarations
//
static bool bmp180_informUser_Impl(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure);

// Set default implementation .. User gets result as bmp180_result_t event
bool (*bmp180_informUser)(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure) = bmp180_informUser_Impl;

// I2C Driver Task
static void bmp180_driver_task(void *pvParameters)
{
    // Data to be received from user
    bmp180_command_t current_command;
    bmp180_constants_t bmp180_constants;
    i2c_dev_t *dev = (i2c_dev_t*)pvParameters;

#ifdef BMP180_DEBUG
    // Wait for commands from the outside
    printf("%s: Started Task\n", __FUNCTION__);
#endif

    // Initialize all internal constants.
    if (!bmp180_fillInternalConstants(dev, &bmp180_constants)) {
        printf("%s: reading internal constants failed\n", __FUNCTION__);
        vTaskDelete(NULL);
    }

    while(1) {
        // Wait for user to insert commands
        if (xQueueReceive(bmp180_rx_queue[dev->bus], &current_command, portMAX_DELAY) == pdTRUE) {
#ifdef BMP180_DEBUG
            printf("%s: Received user command %d 0x%p\n", __FUNCTION__, current_command.cmd, current_command.resultQueue);
#endif
            // use user provided queue
            if (current_command.resultQueue != NULL) {
                // Work on it ...
                int32_t T = 0;
                uint32_t P = 0;

                if (bmp180_measure(dev, &bmp180_constants, &T, (current_command.cmd & BMP180_PRESSURE) ? &P : NULL, 3)) {
                    // Inform the user ...
                    if (!bmp180_informUser(current_command.resultQueue,
                                           current_command.cmd,
                                           ((bmp180_temp_t)T)/10.0,
                                           (bmp180_press_t)P)) {
                        // Failed to send info to user
                        printf("%s: Unable to inform user bmp180_informUser returned \"false\"\n", __FUNCTION__);
                    }
                }
            }
        }
    }
}

static bool bmp180_create_communication_queues(i2c_dev_t *dev)
{
    // Just create them once by bus
    if (bmp180_rx_queue[dev->bus] == NULL)
        bmp180_rx_queue[dev->bus] = xQueueCreate(BMP180_RX_QUEUE_SIZE, sizeof(bmp180_result_t));

    return bmp180_rx_queue[dev->bus] != NULL;
}

static bool bmp180_createTask(i2c_dev_t *dev)
{
    // We already have a task
    portBASE_TYPE x = pdPASS;

    if (bmp180_task_handle[dev->bus] == NULL) {
        x = xTaskCreate(bmp180_driver_task, "bmp180_driver_task", 256, (void*)dev, BMP180_TASK_PRIORITY, &bmp180_task_handle[dev->bus]);  //TODO: name task with i2c bus
    }
    return x == pdPASS;
}

// Default user inform implementation
static bool bmp180_informUser_Impl(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure)
{
    bmp180_result_t result;

    result.cmd = cmd;
    result.temperature = temperature;
    result.pressure = pressure;

    return (xQueueSend(*resultQueue, &result, 0) == pdTRUE);
}

// Just init all needed queues
bool bmp180_init(i2c_dev_t *dev)
{
    // 1. Create required queues
    bool result = false;

    if (bmp180_create_communication_queues(dev)) {
        // 2. Check for bmp180 ...
        if (bmp180_is_available(dev)) {
            // 3. Start driver task
            if (bmp180_createTask(dev)) {
                // We are finished
                result = true;
            }
        }
    }
    return result;
}

void bmp180_trigger_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue)
{
    bmp180_command_t c;

    c.cmd = BMP180_PRESSURE + BMP180_TEMPERATURE;
    c.resultQueue = resultQueue;

    xQueueSend(bmp180_rx_queue[dev->bus], &c, 0);
}


void bmp180_trigger_pressure_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue)
{
    bmp180_command_t c;

    c.cmd = BMP180_PRESSURE;
    c.resultQueue = resultQueue;

    xQueueSend(bmp180_rx_queue[dev->bus], &c, 0);
}

void bmp180_trigger_temperature_measurement(i2c_dev_t *dev, const QueueHandle_t* resultQueue)
{
    bmp180_command_t c;

    c.cmd = BMP180_TEMPERATURE;
    c.resultQueue = resultQueue;

    xQueueSend(bmp180_rx_queue[dev->bus], &c, 0);
}
