# lwIP - A Lightweight TCPIP stack - for esp-open-rtos.

This is fork of git://git.savannah.nongnu.org/lwip.git with a few patches
required to support esp-open-rtos.

Some of the changes are based on the LwIP code drop done by Espressif in their
SDK 0.9.4 which were initially integrated into esp-open-rtos in
https://github.com/SuperHouse/esp-lwip.git

## Status

Some changes to the `struct netif` definition are needed to be consistent with
usage in the binary esp sdk. Many of the code uses have been converted to source
code, so some progress has been made, but the offsets of the `state`, and
`hwaddr` slots, must be at offsets expected by the binary sdk for now.


## Features added

 - `ESP_TIMEWAIT_THRESHOLD` - available heap memory is checked on each TCP
   connection accepted. If it's below this threshold, memory is
   reclaimed by killing TCP connections in TIME-WAIT state.
