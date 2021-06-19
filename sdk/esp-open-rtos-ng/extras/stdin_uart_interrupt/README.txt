This module adds interrupt driven receive on UART 0. Using semaphores, a thread
calling read(...) when no data is available will block in an RTOS expected
manner until data arrives. 

This allows for a background thread running a serial terminal in your program
for debugging and state inspection consuming no CPU cycles at all. Not using
this module will make that thread while(1) until data arrives.

No code changes are needed for adding this module, all you need to do is to add
it to EXTRA_COMPONENTS and add the directive configUSE_COUNTING_SEMAPHORES from
FreeRTOSConfig.h in examples/terminal to your project.