
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <espressif/esp_common.h>
//#include <c_types.h>
#include <etstimer.h>
#include <espressif/esp_system.h>
#include <espressif/esp_timer.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_misc.h>
#include <espressif/osapi.h>

#include <espressif/esp8266/eagle_soc.h>
#include <espressif/esp8266/gpio_register.h>
#include <espressif/esp8266/pin_mux_register.h>

#define LEDGPIO 2
#define BTNGPIO 0

#ifndef ESP32
static ETSTimer resetBtntimer;
#endif

void ioLed(int ena) {
#ifndef ESP32
	//gpio_output_set is overkill. ToDo: use better mactos
	if (ena) {
		sdk_gpio_output_set((1<<LEDGPIO), 0, (1<<LEDGPIO), 0);
	} else {
		sdk_gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
	}
#endif
}

#ifndef ESP32
static void resetBtnTimerCb(void *arg) {
	static int resetCnt=0;
	if ((GPIO_REG_READ(GPIO_IN_ADDRESS)&(1<<BTNGPIO))==0) {
		resetCnt++;
	} else {
		if (resetCnt>=6) { //3 sec pressed
			sdk_wifi_station_disconnect();
			sdk_wifi_set_opmode(STATIONAP_MODE); //reset to AP+STA mode
			printf("Reset to AP mode. Restarting system...\n");
			sdk_system_restart();
		}
		resetCnt=0;
	}
}
#endif


void ioInit() {
#ifndef ESP32
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	sdk_gpio_output_set(0, 0, (1<<LEDGPIO), (1<<BTNGPIO));
	sdk_os_timer_disarm(&resetBtntimer);
	sdk_os_timer_setfn(&resetBtntimer, resetBtnTimerCb, NULL);
	sdk_os_timer_arm(&resetBtntimer, 500, 1);
#endif
}

