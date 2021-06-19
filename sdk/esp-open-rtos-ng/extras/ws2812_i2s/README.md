# WS2812 led driver

This driver uses I2S and DMA subsystems to drive WS2812 leds.
The idea to use I2S to control WS2812 leds belongs to [CNLohr](https://github.com/CNLohr).

## Pros

 * Not using CPU to generate pulses.
 * Interrupt neutral. Reliable operation even with high network load.

## Cons
 
 * Using RAM for DMA buffer. 12 bytes per pixel.
 * Can not change output PIN. Use I2S DATA output pin which is GPIO3.

