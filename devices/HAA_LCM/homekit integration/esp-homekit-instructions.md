### This assumes you are using Maxim Kulkin's esp-homekit library for HomeKit support

- First, add the files ota-api.c and ota-api.h next to your main.c file
- next add extras/rboot-ota to the Makefile
```
EXTRA_COMPONENTS = \
    extras/http-parser \
    extras/dhcpserver \
    extras/rboot-ota \
    $(abspath esp-wolfssl) \
    $(abspath esp-cjson) \
    $(abspath esp-homekit) \
    $(abspath esp-wifi-config)
```
- inside main.c  you should start with adding this section soon after #include section
```
// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

// next use these two lines before calling homekit_server_init(&config);
//    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
//                                      &model.value.string_value,&revision.value.string_value);
//    config.accessories[0]->config_number=c_hash;
// end of OTA add-in instructions
```
###  for example it could end up like this:

##### !! Be aware that if in the future you change the id's of the characteristics (not only ota_trigger) your device might be rejected by the iPhone and it has proven impossible safe re-flash and re-pair to get the device back into action!! So, if after this you decide to add even more, add it behind what you already had.

```
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_window_covering,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "basic-curtain"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Curtain"),
                    &target,
                    &current,
                    &state,
                    &ota_trigger,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {.accessories = accessories, .password = "111-11-111"};

void on_wifi_ready() {
    device_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
```
The cheat line can be used in the initial stage before LCM is used at all since without it homekit will not work