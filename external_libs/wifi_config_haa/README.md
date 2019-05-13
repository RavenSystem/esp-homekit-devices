# esp-wifi-config
Library for esp-open-rtos to bootstrap WiFi-enabled accessories WiFi config

Library uses sysparams to store configuration. When you initialize it
it tries to connect to configured WiFi network. If no configuration exists or
network is not available, it starts it's own WiFi AP (with given name and
optional password). AP runs a captive portal, so when user connects to it a
popup window is displayed asking user to select one of WiFi networks that are
present in that location (and a password if network is secured) and configures
device to connect to that network.

After successful connection it calls provided callback so you can continue
accessory initializiation.

# Example: ::

    #include <stdio.h>
    #include <esp/uart.h>

    #include "wifi_config.h"


    void on_wifi_ready() {
        printf("Connected to WiFi\n");
    }

    void user_init(void) {
        uart_set_baud(0, 115200);

        wifi_config_init("my-accessory", "my-password", on_wifi_ready);
    }

# UI Development

UI content is located in content/index.html (which is actually Jinja2 template).
To simplify UI development this there is a simple server you can use to see
how HTML will be rendered. To run it, you will need Python runtime and Flask python
package.

    pip install flask

Then run server with

    tools/server.py

and connect to http://localhost:5000/settings with your browser. That URL shows
how settings page will look like with some combination of secure &amp; unsecure
networks. http://localhost:5000/settings0 URL shows page when no WiFi networks
could be found.

On build template code will be split into parts (marked with `<!-- part PART_NAME
-->` comments). In all parts all Jinja code blocks (`{% %}`) are removed and all
output blocks (`{{ }}`) are replaced with `%s`. HTML_SETTINGS_HEADER and
HTML_SETTINGS_FOOTER parts are output as-is while HTML_NETWORK_ITEM is assumed to
have two `%s` placeholders, first of which will be "secure" or "unsecure" and
second one - name of a WiFi network.
