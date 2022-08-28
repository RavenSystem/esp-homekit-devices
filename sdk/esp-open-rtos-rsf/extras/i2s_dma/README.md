# Wrapper around hardware I2S and DMA subsystems of ESP8266

ESP8266 has hardware I2S bus support. I2S is a serial bus interface used for
connecting digital audio devices. But can be used to produce sequence of pulses
with reliable timings for example to control a strip of WS2812 leds.

This library is just a wrapper around tricky I2S initialization.
It sets necessary registers, enables I2S clock etc.
