#ifndef MY92XX_H
#define MY92XX_H

#include "haa_lightbulb_driver.h"


typedef enum my92xx_model {
        MY92XX_MODEL_MY9291 = 0X00,
        MY92XX_MODEL_MY9231 = 0X01,
} my92xx_model_t;

typedef enum my92xx_cmd_one_shot_t {
        MY92XX_CMD_ONE_SHOT_DISABLE = 0X00,
        MY92XX_CMD_ONE_SHOT_ENFORCE = 0X01,
} my92xx_cmd_one_shot_t;

typedef enum my92xx_cmd_reaction_t {
        MY92XX_CMD_REACTION_FAST = 0X00,
        MY92XX_CMD_REACTION_SLOW = 0X01,
} my92xx_cmd_reaction_t;

typedef enum my92xx_cmd_bit_width_t {
        MY92XX_CMD_BIT_WIDTH_16 = 0X00,
        MY92XX_CMD_BIT_WIDTH_14 = 0X01,
        MY92XX_CMD_BIT_WIDTH_12 = 0X02,
        MY92XX_CMD_BIT_WIDTH_8 = 0X03,
} my92xx_cmd_bit_width_t;

typedef enum my92xx_cmd_frequency_t {
        MY92XX_CMD_FREQUENCY_DIVIDE_1 = 0X00,
        MY92XX_CMD_FREQUENCY_DIVIDE_4 = 0X01,
        MY92XX_CMD_FREQUENCY_DIVIDE_16 = 0X02,
        MY92XX_CMD_FREQUENCY_DIVIDE_64 = 0X03,
} my92xx_cmd_frequency_t;

typedef enum my92xx_cmd_scatter_t {
        MY92XX_CMD_SCATTER_APDM = 0X00,
        MY92XX_CMD_SCATTER_PWM = 0X01,
} my92xx_cmd_scatter_t;

typedef struct {
        my92xx_cmd_scatter_t scatter:1;
        my92xx_cmd_frequency_t frequency:2;
        my92xx_cmd_bit_width_t bit_width:MY92XX_CMD_BIT_WIDTH_12;
        my92xx_cmd_reaction_t reaction:1;
        my92xx_cmd_one_shot_t one_shot:1;
        unsigned char resv:1;
} __attribute__ ((aligned(1), packed)) my92xx_cmd_t;

#define MY92XX_COMMAND_DEFAULT { \
    .scatter = MY92XX_CMD_SCATTER_APDM, \
    .frequency = MY92XX_CMD_FREQUENCY_DIVIDE_1, \
    .bit_width = MY92XX_CMD_BIT_WIDTH_16, \
    .reaction = MY92XX_CMD_REACTION_FAST, \
    .one_shot = MY92XX_CMD_ONE_SHOT_DISABLE, \
    .resv = 0 \
}

void my92xx_init(driver_interface_t** interface, my92xx_model_t model, uint8_t numChips, uint8_t di_pin, uint8_t dcki_pin, const my92xx_cmd_t* command);
void my92xx_setChannel(driver_descriptor_t* descriptor, uint8_t channel, uint16_t value);
void my92xx_send(driver_descriptor_t* descriptor);

#endif