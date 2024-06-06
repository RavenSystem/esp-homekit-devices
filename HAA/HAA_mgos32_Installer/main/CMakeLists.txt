idf_component_register(
    SRC_DIRS
        "."
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_wifi
        esp_netif
        nvs_flash
        app_update
        http_parser
        driver
        form_urlencoded
        timers_helper
        esp32_port
        adv_logger
        wolfssl
)

# rm -f setup.html.h; python3 ../../common/embed.py setup.html > setup.html.h
