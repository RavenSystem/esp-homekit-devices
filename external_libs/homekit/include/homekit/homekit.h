#ifndef __HOMEKIT_H__
#define __HOMEKIT_H__

#include <homekit/types.h>

struct _client_context_t;
typedef struct _client_context_t client_context_t;


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

    // Callback for "POST /resource" to get snapshot image from camera
    void (*on_resource)(client_context_t *context);

    void (*on_client_connect)(client_context_t *context);
    void (*on_client_disconnect)(client_context_t *context);
} homekit_server_config_t;

// Initialize HomeKit accessory server
void homekit_server_init(homekit_server_config_t *config);

// Reset HomeKit accessory server, removing all pairings
void homekit_server_reset();

client_context_t *homekit_client_get();

void homekit_client_data_set(client_context_t *context, unsigned int data_id, void *data);
void *homekit_client_data_get(client_context_t *context, unsigned int data_id);
void homekit_client_data_delete(client_context_t *context, unsigned int data_id);

unsigned char *homekit_client_get_request_body(client_context_t *context);
size_t homekit_client_get_request_body_size(client_context_t *context);

void homekit_client_send(client_context_t *context, unsigned char *data, size_t size);

#endif // __HOMEKIT_H__
