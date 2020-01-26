"""A simple example how to use PBKDF PKCS #12 algorithm."""

import wolfssl
import os
import random
import string


PASSWORD_LENGTH = 16
SALT_LENGTH = 8
KEY_LENGTH = 16
ITERATIONS = 256
SHA256 = 2  # Hashtype, stands for Sha256 in wolfssl.


def to_c_byte_array(content):
    output = wolfssl.byteArray(len(content))
    for i, ch in enumerate(content):
        output[i] = ord(ch)
    return output


password = ''.join(random.choice(string.ascii_uppercase + string.digits) for x in range(PASSWORD_LENGTH))
salt = os.urandom(SALT_LENGTH)
key = wolfssl.byteArray(KEY_LENGTH)

# params:
# key :: bytearray output
# passwd :: bytearray password that is used to derive the key
# pLen :: password length
# salt :: bytearray salt
# sLen :: salt length
# iterations :: number of iterations
# kLen :: key length
# hashType :: int, SHA256 stands for 2
# purpose :: int, not really sure what it does, 1 was used in the tests
wolfssl.wc_PKCS12_PBKDF(key, to_c_byte_array(password), PASSWORD_LENGTH, to_c_byte_array(salt), SALT_LENGTH, ITERATIONS,
                        KEY_LENGTH, SHA256, 1)
key = wolfssl.cdata(key, KEY_LENGTH)
assert len(key) == KEY_LENGTH, "Generated key has length %s, whereas should have length %s" % (len(key), KEY_LENGTH)

print 'Generated key: %s\nfor password: %s' % (key, password)
print 'Bytes:'
print [b for b in key]
