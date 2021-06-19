/*
 *
 * Test program to verify HMAC test vectors from RFC2202
 * http://www.rfc-editor.org/rfc/rfc2202.txt
 *
 * Currently only does MD5 vectors not SHA1.
 *
 * This sample code is in the public domain.,
 */
#include "espressif/esp_common.h"
#include "esp/uart.h"
#include "FreeRTOS.h"
#include "mbedtls/md.h"

#include <string.h>

struct test_vector {
    const uint8_t *key;
    const uint8_t key_len;
    const uint8_t *data;
    const uint8_t data_len;
    const uint8_t *digest;
};

static const uint8_t aa_80_times[] = {0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,
				      0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa};

const uint8_t NUM_MD5_VECTORS = 7;

static const struct test_vector md5_vectors[] = {
    { /* vector 1*/
	.key =           (const uint8_t *)"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
	.key_len =       16,
	.data =          (const uint8_t *)"Hi There",
	.data_len =      8,
	.digest =        (const uint8_t *)"\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc\x9d",
    },
    { /* vector 2 */
	.key =           (const uint8_t *)"Jefe",
	.key_len =       4,
	.data =          (const uint8_t *)"what do ya want for nothing?",
	.data_len =      28,
	.digest =        (const uint8_t *)"\x75\x0c\x78\x3e\x6a\xb0\xb5\x03\xea\xa8\x6e\x31\x0a\x5d\xb7\x38",
    },
    { /* vector 3 */
	.key =           (const uint8_t *)"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa",
	.key_len =       16,
	.data =          (const uint8_t *)"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd",
	.data_len =      50,
	.digest =        (const uint8_t *)"\x56\xbe\x34\x52\x1d\x14\x4c\x88\xdb\xb8\xc7\x33\xf0\xe8\xb3\xf6",
    },
    { /* vector 4*/
	.key =           (const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19",
	.key_len =         25,
	.data =          (const uint8_t *)"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd",
	.data_len =      50,
	.digest =        (const uint8_t *)"\x69\x7e\xaf\x0a\xca\x3a\x3a\xea\x3a\x75\x16\x47\x46\xff\xaa\x79",
    },
    { /* test case 5 */
	.key =           (const uint8_t *)"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c",
	.key_len =       16,
	.data =          (const uint8_t *)"Test With Truncation",
	.data_len =      20,
	.digest =        (const uint8_t *)"\x56\x46\x1e\xf2\x34\x2e\xdc\x00\xf9\xba\xb9\x95\x69\x0e\xfd\x4c",
    },
    { /* test case 6 */
	.key =           aa_80_times,
	.key_len =       80,
	.data =          (const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First",
	.data_len =      54,
	.digest =        (const uint8_t *)"\x6b\x1a\xb7\xfe\x4b\xd7\xbf\x8f\x0b\x62\xe6\xce\x61\xb9\xd0\xcd",
    },
    {
	/* test case 7 */
	.key =           aa_80_times,
	.key_len =       80,
	.data =          (const uint8_t *)"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data",
	.data_len =      73,
	.digest =        (const uint8_t *)"\x6f\x63\x0f\xad\x67\xcd\xa0\xee\x1f\xb1\xf5\x62\xdb\x3a\xa5\x3e",
    },
};

static void print_blob(const char *label, const uint8_t *data, const uint32_t len)
{
    printf("%s:", label);
    for(int i = 0; i < len; i++) {
        if(i % 16 == 0)
            printf("\n%02x:", i);
        printf(" %02x", data[i]);
    }
}

static void test_md5(void)
{
    printf("\r\nTesting MD5 vectors...\r\n");

    const mbedtls_md_info_t *md5_hmac = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);

    for(int i = 0; i < NUM_MD5_VECTORS; i++) {
	const struct test_vector *vector = &md5_vectors[i];
	printf("Test case %d: ", i+1);

        uint8_t test_digest[16];
        mbedtls_md_hmac(md5_hmac, vector->key, vector->key_len, vector->data, vector->data_len, test_digest);

	if(memcmp(vector->digest, test_digest, 16)) {
	    uint8_t first = 0;
	    for(first = 0; first < 16; first++) {
		if(test_digest[first] != vector->digest[first]) {
		    break;
		}
	    }
	    printf("FAIL, first difference at byte %d\r\n", first);
	    print_blob("key", vector->key, vector->key_len);
	    print_blob("data", vector->data, vector->data_len);
	    print_blob("expected digest", vector->digest, 16);
	    print_blob("actual digest", test_digest, 16);
	}
	else {
	    printf("PASS\r\n");
	}
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);

    test_md5();

    printf("Done.\r\n");
    while(1) {
    }
}
