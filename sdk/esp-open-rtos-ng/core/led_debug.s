/*
 * Useful debugging macro to .include for blinking LEDs on/off from assembler code
 * (aka 1 bit debugging, yay!)
 *
 * To have this work from initial reset, without needing an iomux call
 * first, choose a pin where iomux defaults to GPIO (ie 0,2,4,5)
 *
 * Current sets on=LOW, as the GPIO2 pin is active low
 */
LED_GPIO=2
GPIO_DIR_SET = 0x6000030c
GPIO_OUT_SET = 0x60000300
GPIO_OUT_CLEAR = 0x60000308

.macro led_op rega, regb, target
	movi \rega, GPIO_DIR_SET
	movi \regb, (1<<LED_GPIO)
	s32i \regb, \rega, 0
	movi \rega, \target
	s32i \regb, \rega, 0
.endm

// Turn LED on. rega, regb will be clobbered
.macro led_off rega, regb
	led_op \rega, \regb, GPIO_OUT_SET
.endm

// Turn LED on. rega, regb will be clobbered
.macro led_on rega, regb
	led_op \rega, \regb, GPIO_OUT_CLEAR
.endm
