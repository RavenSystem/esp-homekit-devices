/*
 * Home Accessory Architect
 *
 * Copyright 2019-2020 José Antonio Jiménez Campos (@RavenSystem)
 *
 */

#ifndef __HAA_EXTRA_CHARACTERISTICS__
#define __HAA_EXTRA_CHARACTERISTICS__

#define HOMEKIT_MIAU_UUID(value)        (value "-079E-48FF-8F27-9C2605A29F52")

#define HOMEKIT_CHARACTERISTIC_MIAU_VOLT HOMEKIT_MIAU_UUID("E863F10A")
#define HOMEKIT_DECLARE_CHARACTERISTIC_MIAU_VOLT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_MIAU_VOLT, \
    .description = "Voltage V", \
    .format = HOMETKIT_FORMAT_FLOAT, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY, \
    .min_value = (float[]) {-100000}, \
    .max_value = (float[]) {100000}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_MIAU_AMPERE HOMEKIT_MIAU_UUID("E863F126")
#define HOMEKIT_DECLARE_CHARACTERISTIC_MIAU_AMPERE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_MIAU_AMPERE, \
    .description = "Current A", \
    .format = HOMETKIT_FORMAT_FLOAT, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY, \
    .min_value = (float[]) {-100000}, \
    .max_value = (float[]) {100000}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_MIAU_WATT HOMEKIT_MIAU_UUID("E863F10D")
#define HOMEKIT_DECLARE_CHARACTERISTIC_MIAU_WATT(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_MIAU_WATT, \
    .description = "Power W", \
    .format = HOMETKIT_FORMAT_FLOAT, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY, \
    .min_value = (float[]) {-100000}, \
    .max_value = (float[]) {100000}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__


#define HOMEKIT_CUSTOM_UUID(value)      (value "-0218-2017-81BF-AF2B7C833922")

#define HOMEKIT_SERVICE_CUSTOM_SETUP HOMEKIT_CUSTOM_UUID("F0000100")

#define HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_UPDATE HOMEKIT_CUSTOM_UUID("F0000101")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_OTA_UPDATE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_OTA_UPDATE, \
    .description = "Update", \
    .format = HOMETKIT_FORMAT_STRING, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_PAIRED_WRITE \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .value = HOMEKIT_STRING_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_ENABLE_SETUP HOMEKIT_CUSTOM_UUID("F0000102")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_ENABLE_SETUP(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_ENABLE_SETUP, \
    .description = "Setup", \
    .format = HOMETKIT_FORMAT_STRING, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_PAIRED_WRITE \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .value = HOMEKIT_STRING_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP HOMEKIT_CUSTOM_UUID("F0000901")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CONSUMP(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP, \
    .description = "Consumption KWh", \
    .format = HOMETKIT_FORMAT_FLOAT, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .min_value = (float[]) {0}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_RESET_DATE HOMEKIT_CUSTOM_UUID("F0000902")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CONSUMP_RESET_DATE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_RESET_DATE, \
    .description = "Last reset", \
    .format = HOMETKIT_FORMAT_STRING, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .value = HOMEKIT_STRING_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_BEFORE_RESET HOMEKIT_CUSTOM_UUID("F0000903")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CONSUMP_BEFORE_RESET(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_BEFORE_RESET, \
    .description = "Last Consumption KWh", \
    .format = HOMETKIT_FORMAT_FLOAT, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_NOTIFY \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .min_value = (float[]) {0}, \
    .value = HOMEKIT_FLOAT_(_value), \
    ##__VA_ARGS__

#define HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_RESET HOMEKIT_CUSTOM_UUID("F0000904")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_CONSUMP_RESET(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_CONSUMP_RESET, \
    .description = "Reset Consumption", \
    .format = HOMETKIT_FORMAT_STRING, \
    .permissions = HOMEKIT_PERMISSIONS_PAIRED_READ \
                | HOMEKIT_PERMISSIONS_PAIRED_WRITE \
                | HOMEKIT_PERMISSIONS_HIDDEN, \
    .value = HOMEKIT_STRING_(_value), \
    ##__VA_ARGS__

#endif  // __HAA_EXTRA_CHARACTERISTICS__
