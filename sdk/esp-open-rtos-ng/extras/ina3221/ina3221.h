/**
 * INA3221 driver for esp-open-rtos.
 *
 * Copyright (c) 2016 Zaltora (https://github.com/Zaltora)
 *
 * MIT Licensed as described in the file LICENSE
 *
 */

#ifndef INA3221_H_
#define INA3221_H_

#include <errno.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <i2c/i2c.h>

#define INA3221_ADDR_0  (0x40)      ///< A0 to GND
#define INA3221_ADDR_1  (0x41)      ///< A0 to Vs+
#define INA3221_ADDR_2  (0x42)      ///< A0 to SDA
#define INA3221_ADDR_3  (0x43)      ///< A0 to SCL

#define INA3221_BUS_NUMBER 3  ///< Number of shunt available

#define INA3221_REG_CONFIG                      (0x00)
#define INA3221_REG_SHUNTVOLTAGE_1              (0x01)
#define INA3221_REG_BUSVOLTAGE_1                (0x02)
#define INA3221_REG_CRITICAL_ALERT_1            (0x07)
#define INA3221_REG_WARNING_ALERT_1             (0x08)
#define INA3221_REG_SHUNT_VOLTAGE_SUM           (0x0D)
#define INA3221_REG_SHUNT_VOLTAGE_SUM_LIMIT     (0x0E)
#define INA3221_REG_MASK                        (0x0F)
#define INA3221_REG_VALID_POWER_UPPER_LIMIT     (0x10)
#define INA3221_REG_VALID_POWER_LOWER_LIMIT     (0x11)

/**
 *  Default register after reset
 */
#define INA3221_DEFAULT_CONFIG                   (0x7127)
#define INA3221_DEFAULT_MASK                     (0x0002)
#define INA3221_DEFAULT_POWER_UPPER_LIMIT        (0x2710) //10V
#define INA3221_DEFAULT_POWER_LOWER_LIMIT        (0x2328) //9V

#define INA3221_MASK_CONFIG (0x7C00)

/**
 * Number of samples
 */
typedef enum {
	INA3221_AVG_1 = 0,  ///< Default
	INA3221_AVG_4,
	INA3221_AVG_16,
	INA3221_AVG_64,
	INA3221_AVG_128,
	INA3221_AVG_256,
	INA3221_AVG_512,
	INA3221_AVG_1024,
} ina3221_avg_t;

/**
 * Channel selection list
 */
typedef enum {
    CHANNEL_1 = 0,
    CHANNEL_2,
    CHANNEL_3,
} ina3221_channel_t;

/**
 * Conversion time in us
 */
typedef enum {
	INA3221_CT_140 = 0,
	INA3221_CT_204,
	INA3221_CT_332,
	INA3221_CT_588,
	INA3221_CT_1100,  ///< Default
	INA3221_CT_2116,
	INA3221_CT_4156,
	INA3221_CT_8244,
} ina3221_ct_t;

/**
 * Config description register
 */
typedef union
{
    struct {
        uint16_t esht : 1; ///< Enable/Disable shunt measure    // LSB
        uint16_t ebus : 1; ///< Enable/Disable bus measure
        uint16_t mode : 1; ///< Single shot measure or continious mode
        uint16_t vsht : 3; ///< Shunt voltage conversion time
        uint16_t vbus : 3; ///< Bus voltage conversion time
        uint16_t avg  : 3; ///< number of sample collected and averaged together
        uint16_t ch3  : 1; ///< Enable/Disable channel 3
        uint16_t ch2  : 1; ///< Enable/Disable channel 2
        uint16_t ch1  : 1; ///< Enable/Disable channel 1
        uint16_t rst  : 1; ///< Set this bit to 1 to reset device  // MSB
    };
    uint16_t config_register;
} ina3221_config_t;


/**
 * Mask/enable description register
 */
typedef union
{
    struct {
        uint16_t cvrf : 1; ///< Conversion ready flag (1: ready)   // LSB
        uint16_t tcf  : 1; ///< Timing control flag
        uint16_t pvf  : 1; ///< Power valid flag
        uint16_t wf   : 3; ///< Warning alert flag (Read mask to clear) (order : Channel1:channel2:channel3)
        uint16_t sf   : 1; ///< Sum alert flag (Read mask to clear)
        uint16_t cf   : 3; ///< Critical alert flag (Read mask to clear) (order : Channel1:channel2:channel3)
        uint16_t cen  : 1; ///< Critical alert latch (1:enable)
        uint16_t wen  : 1; ///< Warning alert latch (1:enable)
        uint16_t scc3 : 1; ///< channel 3 sum (1:enable)
        uint16_t scc2 : 1; ///< channel 2 sum (1:enable)
        uint16_t scc1 : 1; ///< channel 1 sum (1:enable)
        uint16_t      : 1; ///< Reserved         //MSB
    };
    uint16_t mask_register;
} ina3221_mask_t;

/**
 *  Device description
 */
typedef struct {
    const i2c_dev_t i2c_dev;                  ///< ina3221 I2C address
    const uint16_t shunt[INA3221_BUS_NUMBER]; ///< Memory of shunt value (mOhm)
    ina3221_config_t config;                  ///< Memory of ina3221 config
    ina3221_mask_t mask;                      ///< Memory of mask_config
} ina3221_t;

/**
 * sync internal config buffer  and mask with external device register ( When struct is manually set )
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ina3221_sync(ina3221_t *dev);

/**
 * send current config register to trig a measurement in single-shot mode
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ina3221_trigger(ina3221_t *dev);

/**
 * get mask register from the device ( Used to read flags )
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ina3221_getStatus(ina3221_t *dev);

/**
 * Set options for bus and shunt
 * @param dev Pointer to device descriptor
 * @param mode Selection of measurement (true :continuous // false:single-shot)
 * @param bus Enable/Disable bus measures
 * @param shunt Enable/Disable shunt measures
 * @return Non-zero if error occured
 */
int ina3221_setting(ina3221_t *dev ,bool mode, bool bus, bool shunt);

/**
 * Select channel
 * @param dev Pointer to device descriptor
 * @param ch1 Enable/Disable channel 1 ( true : enable // false : disable )
 * @param ch2 Enable/Disable channel 2 ( true : enable // false : disable )
 * @param ch3 Enable/Disable channel 3 ( true : enable // false : disable )
 * @return Non-zero if error occured
 */
int ina3221_enableChannel(ina3221_t *dev ,bool ch1, bool ch2, bool ch3);

/**
 * Select channel to be sum (don't impact enable channel status)
 * @param dev Pointer to device descriptor
 * @param ch1 Enable/Disable channel 1 ( true : enable // false : disable )
 * @param ch2 Enable/Disable channel 2 ( true : enable // false : disable )
 * @param ch3 Enable/Disable channel 3 ( true : enable // false : disable )
 * @return Non-zero if error occured
 */
int ina3221_enableChannelSum(ina3221_t *dev ,bool ch1, bool ch2, bool ch3);

/**
 * enable Latch on warning and critical alert pin
 * @param dev Pointer to device descriptor
 * @param warning Enable/Disable warning latch ( true : Latch // false : Transparent )
 * @param critical Enable/Disable critical latch ( true : Latch // false : Transparent )
 * @return Non-zero if error occured
 */
int ina3221_enableLatchPin(ina3221_t *dev ,bool warning, bool critical);

/**
 * Set average ( number(s) of point measured )
 * @param dev Pointer to device descriptor
 * @param avg Value of average selection
 * @return Non-zero if error occured
 */
int ina3221_setAverage(ina3221_t *dev, ina3221_avg_t avg);

/**
 * Set conversion time ( time for a measurement ) for bus.
 * @param dev Pointer to device descriptor
 * @param ct Value of conversion time selection
 * @return Non-zero if error occured
 */
int ina3221_setBusConversionTime(ina3221_t *dev,ina3221_ct_t ct);

/**
 * Set conversion time ( time for a measurement ) for shunt.
 * @param dev Pointer to device descriptor
 * @param ct Value of conversion time selection
 * @return Non-zero if error occured
 */
int ina3221_setShuntConversionTime(ina3221_t *dev,ina3221_ct_t ct);

/**
 * Reset device and config like POR (Power-On-Reset)
 * @param dev Pointer to device descriptor
 * @return Non-zero if error occured
 */
int ina3221_reset(ina3221_t *dev);

/**
 * Get Bus voltage (V)
 * @param dev Pointer to device descriptor
 * @param channel Select channel value to get
 * @param voltage Data pointer to get bus voltage (V)
 * @return Non-zero if error occured
 */
int ina3221_getBusVoltage(ina3221_t *dev, ina3221_channel_t channel, float *voltage);

/**
 * Get Shunt voltage (mV) and current (mA)
 * @param dev Pointer to device descriptor
 * @param channel Select channel value to get
 * @param voltage Data pointer to get shunt voltage (mV)
 * @param current Data pointer to get shunt voltage (mA)
 * @return Non-zero if error occured
 */
int ina3221_getShuntValue(ina3221_t *dev, ina3221_channel_t channel, float *voltage, float *current);

/**
 * Get Shunt-voltage (mV) sum value of selected channels
 * @param dev Pointer to device descriptor
 * @param channel Select channel value to get
 * @param voltage Data pointer to get shunt voltage (mV)
 * @return Non-zero if error occured
 */
int ina3221_getSumShuntValue(ina3221_t *dev, float *voltage);

/**
 * Set Critical alert (when measurement(s) is greater that value set )
 * @param dev Pointer to device descriptor
 * @param channel Select channel value to set
 * @param current Value to set (mA) // max : 163800/shunt (mOhm)
 * @return Non-zero if error occured
 */
int ina3221_setCriticalAlert(ina3221_t *dev, ina3221_channel_t channel, float current);

/**
 * Set Warning alert (when average measurement(s) is greater that value set )
 * @param dev Pointer to device descriptor
 * @param channel Select channel value to set
 * @param current Value to set (mA)  // max : 163800/shunt (mOhm)
 * @return Non-zero if error occured
 */
int ina3221_setWarningAlert(ina3221_t *dev, ina3221_channel_t channel, float current);

/**
 * Set Sum Warning alert (Compared to each completed cycle of all selected channels : Sum register )
 * @param dev Pointer to device descriptor
 * @param voltage voltage to set (mV) //  max : 655.32
 * @return Non-zero if error occured
 */
int ina3221_setSumWarningAlert(ina3221_t *dev, float voltage);

/**
 * Set Power-valid upper-limit ( To determine if power conditions are met.)( bus need enable )
 * If bus voltage exceed the value set, PV pin is high
 * @param dev Pointer to device descriptor
 * @param voltage voltage to set (V)
 * @return Non-zero if error occured
 */
int ina3221_setPowerValidUpperLimit(ina3221_t *dev, float voltage);

/**
 * Set Power-valid lower-limit ( To determine if power conditions are met.)( bus need enable )
 * If bus voltage drops below the value set, PV pin is low
 * @param dev Pointer to device descriptor
 * @param voltage voltage to set (V)
 * @return Non-zero if error occured
 */
int ina3221_setPowerValidLowerLimit(ina3221_t *dev, float voltage);

#ifdef __cplusplus
}
#endif

#endif /* INA3221_H_ */
