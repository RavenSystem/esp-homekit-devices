/* The "blink" example, but writing the LED from timer interrupts
 *
 * This sample code is in the public domain.
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"

const int gpio_frc1 = 12;
const int freq_frc1 = 1;

const int gpio_frc2 = 14;
const int freq_frc2 = 10;

static volatile uint32_t frc1_count;
static volatile uint32_t frc2_count;

void frc1_interrupt_handler(void *arg)
{
    frc1_count++;
    gpio_toggle(gpio_frc1);
}

void frc2_interrupt_handler(void *arg)
{
    /* FRC2 needs the match register updated on each timer interrupt */
    timer_set_frequency(FRC2, freq_frc2);
    frc2_count++;
    gpio_toggle(gpio_frc2);
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    /* configure GPIOs */
    gpio_enable(gpio_frc1, GPIO_OUTPUT);
    gpio_enable(gpio_frc2, GPIO_OUTPUT);
    gpio_write(gpio_frc1, 1);

    /* stop both timers and mask their interrupts as a precaution */
    timer_set_interrupts(FRC1, false);
    timer_set_run(FRC1, false);
    timer_set_interrupts(FRC2, false);
    timer_set_run(FRC2, false);

    /* set up ISRs */
    _xt_isr_attach(INUM_TIMER_FRC1, frc1_interrupt_handler, NULL);
    _xt_isr_attach(INUM_TIMER_FRC2, frc2_interrupt_handler, NULL);

    /* configure timer frequencies */
    timer_set_frequency(FRC1, freq_frc1);
    timer_set_frequency(FRC2, freq_frc2);

    /* unmask interrupts and start timers */
    timer_set_interrupts(FRC1, true);
    timer_set_run(FRC1, true);
    timer_set_interrupts(FRC2, true);
    timer_set_run(FRC2, true);

    gpio_write(gpio_frc1, 0);
}
