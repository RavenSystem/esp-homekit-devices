# test data from test.c

import wolfssl

KEY_LENGTH = 24
SHA256     = 2  # Hashtype, stands for Sha256 in wolfssl.


def to_c_byte_array(content):
    output = wolfssl.byteArray(len(content))
    for i, ch in enumerate(content):
        output[i] = ord(ch)
    return output


password = '\x00\x73\x00\x6d\x00\x65\x00\x67\x00\x00'
salt     = '\x0a\x58\xCF\x64\x53\x0d\x82\x3f'
key      = wolfssl.byteArray(KEY_LENGTH)
verify   = '\x27\xE9\x0D\x7E\xD5\xA1\xC4\x11\xBA\x87\x8B\xC0\x90\xF5\xCE\xBE\x5E\x9D\x5F\xE3\xD6\x2B\x73\xAA'

wolfssl.wc_PKCS12_PBKDF(key, to_c_byte_array(password), len(password),
                        to_c_byte_array(salt), len(salt), 1, KEY_LENGTH,
                        SHA256, 1)
key = wolfssl.cdata(key, KEY_LENGTH)
assert key == verify


password = '\x00\x71\x00\x75\x00\x65\x00\x65\x00\x67\x00\x00'
salt     = '\x16\x82\xC0\xfC\x5b\x3f\x7e\xc5'
key      = wolfssl.byteArray(KEY_LENGTH)
verify   = '\x90\x1B\x49\x70\xF0\x94\xF0\xF8\x45\xC0\xF3\xF3\x13\x59\x18\x6A\x35\xE3\x67\xFE\xD3\x21\xFD\x7C'

wolfssl.wc_PKCS12_PBKDF(key, to_c_byte_array(password), len(password),
                        to_c_byte_array(salt), len(salt), 1000, KEY_LENGTH,
                        SHA256, 1)
key = wolfssl.cdata(key, KEY_LENGTH)
assert key == verify
