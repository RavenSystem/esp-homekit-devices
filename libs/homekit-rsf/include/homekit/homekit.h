#ifndef __HOMEKIT_H__
#define __HOMEKIT_H__

#include <homekit/types.h>

typedef void *homekit_client_id_t;

#ifdef HOMEKIT_NOTIFY_EVENT_ENABLE
typedef enum {
    HOMEKIT_EVENT_SERVER_INITIALIZED,
    // Just accepted client connection
    HOMEKIT_EVENT_CLIENT_CONNECTED,
    // Pairing verification completed and secure session is established
    HOMEKIT_EVENT_CLIENT_VERIFIED,
    HOMEKIT_EVENT_CLIENT_DISCONNECTED,
    HOMEKIT_EVENT_PAIRING_ADDED,
    HOMEKIT_EVENT_PAIRING_REMOVED,
} homekit_event_t;
#endif

typedef struct {
    // Pointer to an array of homekit_accessory_t pointers.
    // Array should be terminated by a NULL pointer.
    homekit_accessory_t **accessories;
    
    // Setup ID in format "XXXX" (where X is digit or latin capital letter)
    // Used for pairing using QR code
    char* setup_id;
    
    uint16_t mdns_ttl;
    uint16_t mdns_ttl_period;
    
    uint16_t config_number;
    homekit_device_category_t category: 8;  // 6 bits
    uint8_t max_clients: 6;                 // 5 bits
    bool insecure: 1;
    bool re_pair: 1;
    
#ifdef HOMEKIT_SERVER_ON_RESOURCE_ENABLE
    // Callback for "POST /resource" to get snapshot image from camera
    void (*on_resource)(const char *body, size_t body_size);
#endif
    
#ifdef HOMEKIT_NOTIFY_EVENT_ENABLE
    void (*on_event)(homekit_event_t event);
#endif
    
} homekit_server_config_t;

// Initialize HomeKit accessory server
void homekit_server_init(homekit_server_config_t *config);

// Set maximum connected HomeKit clients simultaneously (max 32)
#ifdef HOMEKIT_CHANGE_MAX_CLIENTS
void homekit_set_max_clients(const unsigned int clients);
#endif // HOMEKIT_CHANGE_MAX_CLIENTS

// Remove oldest client to free some DRAM
void homekit_remove_oldest_client();

// Reset HomeKit accessory server, removing all pairings
void homekit_server_reset();

void homekit_remove_extra_pairing(const unsigned int last_keep);
int homekit_pairing_count();

void homekit_mdns_announce_start();
void homekit_mdns_announce_stop();

int homekit_get_accessory_id(char *buffer, size_t size);
bool homekit_is_pairing();
bool homekit_is_paired();

int32_t homekit_get_unique_client_ipaddr();
int homekit_get_client_count();

// Client related stuff
//homekit_client_id_t homekit_get_client_id();

//bool homekit_client_is_admin();
//int  homekit_client_send(unsigned char *data, size_t size);

#endif // __HOMEKIT_H__
