#include <string.h>
#include <ctype.h>
#include "debug.h"
#include "crypto.h"
#include "pairing.h"
#include "port.h"

#ifdef ESP_PLATFORM

#define SPIFLASH_HOMEKIT_BASE_ADDR  (0)

#else

#ifndef SPIFLASH_HOMEKIT_BASE_ADDR
#error "!!! Define SPIFLASH_HOMEKIT_BASE_ADDR"
#endif

#endif

#define MAGIC_OFFSET            (0)
#define ACCESSORY_ID_OFFSET     (4)
#define ACCESSORY_KEY_OFFSET    (32)
#define PAIRINGS_OFFSET         (128)

#define MAGIC_ADDR              (SPIFLASH_HOMEKIT_BASE_ADDR + MAGIC_OFFSET)
#define ACCESSORY_ID_ADDR       (SPIFLASH_HOMEKIT_BASE_ADDR + ACCESSORY_ID_OFFSET)
#define ACCESSORY_KEY_ADDR      (SPIFLASH_HOMEKIT_BASE_ADDR + ACCESSORY_KEY_OFFSET)
#define PAIRINGS_ADDR           (SPIFLASH_HOMEKIT_BASE_ADDR + PAIRINGS_OFFSET)

#define MAX_PAIRINGS            (40)

#define ACCESSORY_ID_SIZE       (17)
#define ACCESSORY_KEY_SIZE      (64)

const char magic1[] = "HAP";    // Must be 3 chars + null terminator

#ifdef ESP_PLATFORM
esp_partition_t* hap_partition = NULL;

static void enable_hap_partition() {
    if (!hap_partition) {
        hap_partition = esp_partition_find_first(0xFE, 0xFE, NULL);
        
        if (!hap_partition) {
            hap_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
            
            if (!hap_partition) {
                hap_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
            }
        }
    }
}
#else
#define enable_hap_partition()
#endif

int homekit_storage_reset() {
    byte blank[3];
    memset(blank, 0, sizeof(blank));
    
    enable_hap_partition();

#ifdef ESP_PLATFORM
    if (esp_partition_write(hap_partition, MAGIC_ADDR, blank, sizeof(blank)) != ESP_OK) {
#else
    if (!spiflash_write(MAGIC_ADDR, blank, sizeof(blank))) {
#endif
        ERROR("Reset flash");
        return -1;
    }

    return 0;
}


int homekit_storage_init() {
    char magic[sizeof(magic1)];
    memset(magic, 0, sizeof(magic));
    
    enable_hap_partition();
    
    if (!spiflash_read(MAGIC_ADDR, (byte*) magic, sizeof(magic))) {
        ERROR("Read magic");
    }

    if (strncmp(magic, magic1, 2)) {
        if (!spiflash_erase_sector(SPIFLASH_HOMEKIT_BASE_ADDR)) {
            ERROR("Erase flash");
            return -1;
        }
        
        strncpy(magic, magic1, sizeof(magic));
        if (!spiflash_write(MAGIC_ADDR, (byte*) magic, sizeof(magic))) {
            ERROR("Init sec");
            return -1;
        }
        
        return 1;
    }
    
    return 0;
}


void homekit_storage_save_accessory_id(const char *accessory_id) {
    if (!spiflash_write(ACCESSORY_ID_ADDR, (byte *)accessory_id, strlen(accessory_id))) {
        ERROR("Write ID");
    }
}


static char ishex(unsigned char c) {
    c = toupper(c);
    return isdigit(c) || (c >= 'A' && c <= 'F');
}

char *homekit_storage_load_accessory_id() {
    byte data[ACCESSORY_ID_SIZE + 1];
    if (!spiflash_read(ACCESSORY_ID_ADDR, data, sizeof(data))) {
        ERROR("Read ID");
        return NULL;
    }
    if (!data[0])
        return NULL;
    data[sizeof(data) - 1] = 0;

    for (int i = 0; i < ACCESSORY_ID_SIZE; i++) {
        if (i % 3 == 2) {
           if (data[i] != ':')
               return NULL;
        } else if (!ishex(data[i]))
            return NULL;
    }

    return strndup((char *)data, sizeof(data));
}

void homekit_storage_save_accessory_key(const ed25519_key *key) {
    byte key_data[ACCESSORY_KEY_SIZE];
    size_t key_data_size = sizeof(key_data);
    int r = crypto_ed25519_export_key(key, key_data, &key_data_size);
    if (r) {
        ERROR("Export acc key (%d)", r);
        return;
    }

    if (!spiflash_write(ACCESSORY_KEY_ADDR, key_data, key_data_size)) {
        ERROR("Write acc key");
        return;
    }
}

ed25519_key *homekit_storage_load_accessory_key() {
    byte key_data[ACCESSORY_KEY_SIZE];
    if (!spiflash_read(ACCESSORY_KEY_ADDR, key_data, sizeof(key_data))) {
        ERROR("Read acc key");
        return NULL;
    }

    ed25519_key *key = crypto_ed25519_new();
    int r = crypto_ed25519_import_key(key, key_data, sizeof(key_data));
    if (r) {
        ERROR("Import acc key (%d)", r);
        crypto_ed25519_free(key);
        return NULL;
    }

    return key;
}

// TODO: figure out alignment issues
typedef struct {
    char magic[sizeof(magic1)];     // 4  bytes (3 chars + null erminator)
    byte permissions;               // 1  bytes
    char device_id[36];             // 36 bytes
    byte device_public_key[32];     // 32 bytes

    byte _reserved[7];              // 7  bytes
                                    // Align record to be 80 bytes
} pairing_data_t;


bool homekit_storage_can_add_pairing() {
    pairing_data_t data;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                return true;
            }
        }
    }
    return false;
}

static int compact_data() {
    INFO("Compacting data");
    
    byte *data = malloc(SPI_FLASH_SECTOR_SIZE);
    if (!spiflash_read(SPIFLASH_HOMEKIT_BASE_ADDR, data, SPI_FLASH_SECTOR_SIZE)) {
        free(data);
        ERROR("Compact read");
        return -1;
    }
    
    unsigned int next_pairing_idx = 0;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        pairing_data_t *pairing_data = (pairing_data_t *)&data[PAIRINGS_OFFSET + sizeof(pairing_data_t) * i];
        if (!strncmp(pairing_data->magic, magic1, sizeof(magic1))) {
            if (i != next_pairing_idx) {
                memcpy(&data[PAIRINGS_OFFSET + sizeof(pairing_data_t) * next_pairing_idx],
                       pairing_data, sizeof(*pairing_data));
            }
            next_pairing_idx++;
        }
    }

    if (next_pairing_idx == MAX_PAIRINGS) {
        // We are full, no compaction possible, do not waste flash erase cycle
        return 0;
    }

    if (homekit_storage_reset() != 0) {
        ERROR("Compact resetting");
        free(data);
        return -1;
    }
    if (homekit_storage_init() < 0) {
        ERROR("Compact initializing");
        free(data);
        return -1;
    }

    if (!spiflash_write(SPIFLASH_HOMEKIT_BASE_ADDR, data, PAIRINGS_OFFSET + sizeof(pairing_data_t) * next_pairing_idx)) {
        ERROR("Compact writing");
        free(data);
        return -1;
    }

    free(data);
    return 0;
}

static int find_empty_block() {
    byte data[sizeof(pairing_data_t)];

    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, data, sizeof(data))) {
            
            bool block_empty = true;
            for (unsigned int j = 0; j < sizeof(data); j++)
                if (data[j] != 0xff) {
                    block_empty = false;
                    break;
                }
            
            if (block_empty) {
                return i;
            }
        }
    }

    return -1;
}

int homekit_storage_add_pairing(const char *device_id, const ed25519_key *device_key, byte permissions) {
    int next_block_idx = find_empty_block();
    if (next_block_idx == -1) {
        compact_data();
        next_block_idx = find_empty_block();
    }

    if (next_block_idx == -1) {
        ERROR("Write pairing: max");
        return -2;
    }

    pairing_data_t data;

    memset(&data, 0, sizeof(data));
    strncpy(data.magic, magic1, sizeof(magic1));
    data.permissions = permissions;
    memset(data.device_id, 0, sizeof(data.device_id));
    memcpy(data.device_id, device_id, sizeof(data.device_id));
    size_t device_public_key_size = sizeof(data.device_public_key);
    int r = crypto_ed25519_export_public_key(
        device_key, data.device_public_key, &device_public_key_size
    );
    if (r) {
        ERROR("Export dev pub key (%d)", r);
        return -1;
    }
    
    if (!spiflash_write(PAIRINGS_ADDR + sizeof(data)*next_block_idx, (byte *)&data, sizeof(data))) {
        ERROR("Write pairing");
        return -1;
    }
    
    return 0;
}


int homekit_storage_update_pairing(const char *device_id, const ed25519_key *device_key, byte permissions) {
    pairing_data_t data;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                continue;
            }
            
            if (!strncmp(data.device_id, device_id, sizeof(data.device_id))) {
                if (data.permissions != permissions) {
                    memset(&data, 0, sizeof(data));
                    if (!spiflash_write(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
                        ERROR("Erase old pairing");
                        return -2;
                    }
                    
                    return homekit_storage_add_pairing(device_id, device_key, permissions);
                }
                
                return 0;
            }
        }
    }
    
    return -1;
}


int homekit_storage_remove_pairing(const char *device_id) {
    pairing_data_t data;
    for (int i = 0; i<MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                continue;
            }
            
            if (!strncmp(data.device_id, device_id, sizeof(data.device_id))) {
                memset(&data, 0, sizeof(data));
                if (!spiflash_write(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
                    ERROR("Remove pairing");
                    return -2;
                }
                
                return 0;
            }
        }
    }
    
    return 0;
}

unsigned int homekit_storage_pairing_count() {
    enable_hap_partition();
    
    pairing_data_t data;
    unsigned int count = 0;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                continue;
            }
            
            count++;
        }
    }
    
    return count;
}

int homekit_storage_remove_extra_pairing(const unsigned int last_keep) {
    enable_hap_partition();
    
    pairing_data_t data;
    unsigned int count = 0;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                continue;
            }
            
            count++;
            
            if (count > last_keep) {
                memset(&data, 0, sizeof(data));
                if (!spiflash_write(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
                    ERROR("Remove pairing");
                    return -2;
                }
            }
        }
    }
    
    return 0;
}


pairing_t *homekit_storage_find_pairing(const char *device_id) {
    pairing_data_t data;
    for (unsigned int i = 0; i < MAX_PAIRINGS; i++) {
        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * i, (byte *)&data, sizeof(data))) {
            if (strncmp(data.magic, magic1, sizeof(magic1))) {
                continue;
            }
            
            if (!strncmp(data.device_id, device_id, sizeof(data.device_id))) {
                ed25519_key *device_key = crypto_ed25519_new();
                int r = crypto_ed25519_import_public_key(device_key, data.device_public_key, sizeof(data.device_public_key));
                if (r) {
                    ERROR("Import dev pub key (%d)", r);
                    return NULL;
                }
                
                pairing_t *pairing = pairing_new();
                pairing->id = i;
                pairing->device_id = strndup(data.device_id, sizeof(data.device_id));
                pairing->device_key = device_key;
                pairing->permissions = data.permissions;
                
                return pairing;
            }
        }
    }

    return NULL;
}


typedef struct {
    int idx;
} pairing_iterator_t;


pairing_iterator_t *homekit_storage_pairing_iterator() {
    pairing_iterator_t *it = malloc(sizeof(pairing_iterator_t));
    it->idx = 0;
    return it;
}


pairing_t *homekit_storage_next_pairing(pairing_iterator_t *it) {
    pairing_data_t data;
    while(it->idx < MAX_PAIRINGS) {
        int id = it->idx;
        it->idx++;

        if (spiflash_read(PAIRINGS_ADDR + sizeof(data) * id, (byte *)&data, sizeof(data))) {
            if (!strncmp(data.magic, magic1, sizeof(magic1))) {
                ed25519_key *device_key = crypto_ed25519_new();
                int r = crypto_ed25519_import_public_key(device_key, data.device_public_key, sizeof(data.device_public_key));
                if (r) {
                    ERROR("Import dev pub key (%d)", r);
                    crypto_ed25519_free(device_key);
                    continue;
                }
                
                pairing_t *pairing = pairing_new();
                pairing->id = id;
                pairing->device_id = strndup(data.device_id, sizeof(data.device_id));
                pairing->device_key = device_key;
                pairing->permissions = data.permissions;
                
                return pairing;
            }
        }
    }

    return NULL;
}

