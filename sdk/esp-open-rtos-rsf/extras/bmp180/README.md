# Driver for BMP085/BMP180 digital pressure sensor

This driver is written for usage with the ESP8266 and FreeRTOS ([esp-open-rtos](https://github.com/SuperHouse/esp-open-rtos) and [esp-open-rtos-driver-i2c](https://github.com/kanflo/esp-open-rtos-driver-i2c)).

### Usage

Before using the BMP180 module, the function `bmp180_init(SCL_PIN, SDA_PIN)` needs to be called to setup the I2C interface and do validation if the BMP180/BMP085 is accessible.

If the setup is sucessfully and a measurement is triggered, the result of the measurement is provided to the user as an event send via the `qQueue` provided with `bmp180_trigger_*measurement(pQueue);` 

#### Example 

```
#define SCL_PIN GPIO_ID_PIN(0)
#define SDA_PIN GPIO_ID_PIN(2)
...

if (!bmp180_init(SCL_PIN, SDA_PIN)) {
// An error occured, while dong the init (E.g device not found etc.)
}

// Trigger a measurement
bmp180_trigger_measurement(pQueue);

```

#### Change queue event

Per default the event send to the user via the provided queue is of the type `bmp180_result_t`. As this might not always be desired, a way is provided so that the user can provide a function, which creates and sends the event via the provided queue.

As all data aqquired from the BMP180/BMP085 is provided to the `bmp180_informUser` function, it is also possible to calculate new informations (E.g altitude etc.)

##### Example

```
// Own BMP180 User Inform Implementation
bool my_informUser(const QueueHandle_t* resultQueue, uint8_t cmd, bmp180_temp_t temperature, bmp180_press_t pressure) {
	my_event_t ev;

	ev.event_type = MY_EVT_BMP180;
	ev.bmp180_data.cmd = cmd;
	ev.bmp180_data.temperature = temperature;
	ev.bmp180_data.pressure = pressure;

	return (xQueueSend(*resultQueue, &ev, 0) == pdTRUE);
}

...

// Use our user inform implementation
// needs to be set before first measurement is triggered
bmp180_informUser = my_informUser;


``` 

