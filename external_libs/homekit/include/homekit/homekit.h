#ifndef __HOMEKIT_H__
#define __HOMEKIT_H__

#include <homekit/types.h>

typedef struct {
    // Pointer to an array of homekit_accessory_t pointers.
    // Array should be terminated by a NULL pointer.
    homekit_accessory_t **accessories;

    // Password in format "111-23-456".
    // If password is not specified, a random password
    // will be used. In that case, a password_callback
    // field must contain pointer to a function that should
    // somehow communicate password to the user (e.g. display
    // it on a screen if accessory has one).
    char *password;
    void (*password_callback)(const char *password);
} homekit_server_config_t;

// Initialize HomeKit accessory server
void homekit_server_init(homekit_server_config_t *config);

// Reset HomeKit accessory server, removing all pairings
void homekit_server_reset();

#endif // __HOMEKIT_H__
