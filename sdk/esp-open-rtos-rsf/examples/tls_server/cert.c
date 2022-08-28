/* This is the unencrypted RSA 2048-bit private key and certificate to use for the server, in PEM
   format, as dumped via:

   openssl req -x509 -nodes -days 365 -sha256 -newkey rsa:2048 -keyout cert.pem -out cert.pem

   ... and then the relevant parts of the .pem file copied out into the strings below.
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

const char *server_key =
    "-----BEGIN PRIVATE KEY-----\r\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCr5Pq7VARJJKa5\r\n"
    "gl68F2TJARsJ71fp4a48uVPwwtm7L7ili+qxhtB73AOjpiR/ZqJBkpMrSjbNdHPp\r\n"
    "vz9Bk164msv81DXzDuzn5QXuvbI87IFdnCM+I+xLSPCjBbPjPQsm7UJUG9pfjAAZ\r\n"
    "lF4KAN53OLpKOeu6ut7b0lHJ1H/YLeBOMcReTbObYa6QHO4+Rl7n9MMkDg1geCsW\r\n"
    "as6LznPVM1Jy7kh7IZZ3As+4dQ+V3VJ3/t/+lOK5O+WtAWZ96b7AHxc0DkgCLWz9\r\n"
    "+4fpRxHtjD4sA7nXEpgk6qYqS/8N0tHKQBC+uDSWxE3t0mF2Jr1D1gGlFy3VV5r2\r\n"
    "fEfWN74JAgMBAAECggEBAI7gB4v3HIzTOwVMmIOMikgMdDYAy7jpzZJJlLy0qJdO\r\n"
    "5hIrxwqB/P5GdHvsl7+RRmJse4jq6bxCBCqQvPo7jOqyN8VRefoqOL3S/ehfoivD\r\n"
    "hQ+SvTRkVX6KBQHrtoa1cXSMlqokcJEkY9zfFn8IE+FStH0Hwaj2tFBQc4zn5M+A\r\n"
    "jXsfySLT5dbQws/mJwn5DcuBtNqUBaYmUxRVOpiqbidmiszcqOEcfqfbL+fLmmhB\r\n"
    "mMdsKj79yUG98XtAwDo6zYqrt/KZ+Zty/9KIw1KLmYa2Cf6UWTCj4CKyVJuI0vy7\r\n"
    "pqFYC5a5C/MtLt+R6CgbcRey598NkpDZwOyt0BgdKoECgYEA1eDFq9qhMmxKNUtE\r\n"
    "K4e9u90xYNhSdKQV/XyXp+Fk0aSGuDdjLJm+q1i2KD6RfbuxCy41VPNs/EOlp6mP\r\n"
    "+fCTDVSrBLkOS/d2Dib/0XWi3o/Np6ZB9PZbU8tXqqKcaKmkhTOI/P3c+4Ap2xfD\r\n"
    "BtN/GBe5DRO7Nn60va3A0cSF2fECgYEAzb9+kFnEf13XGvk529wgDbl3tHsNrHRZ\r\n"
    "OW+fBNt/5D+H8Ky4m1Hkejk28q77fCQPRvf0LeNKCWq/rDLjjkC7EB5hDztHXFf0\r\n"
    "ptNLyOVnh16hSs4BrzLebfsSqMCYgWGxGJe3udxm+wPnMA5tf0rATY2B0wlZ90ew\r\n"
    "pjjK5BdbTZkCgYA0zLef9GpFG2y6eWlL4cfaQAH3qY+5keSH3qFF5aPRCW/kvG+0\r\n"
    "TARBIrZdewzJ4HMVkoPCBBJMuJqFqJuNlXGIIfXSRakc4et4FPKkkAj0LsYTdDzm\r\n"
    "L4deSV3MFzbLs82UwKM56aYLRJmQp+4SmlXO6dRaQRu/mUofZWyrnHt60QKBgQCC\r\n"
    "5eUAs4vXOH2k9JDB9w8RjEDDO1KcuD0X1JMIBRodvemfzlN4xaYluIbj6T2oYkyx\r\n"
    "6wiXtTYiPZ8KUCoEE9yvSZSYmy8waekFxgI+Iu0165eUPvJFY4it0gGyCS49ikig\r\n"
    "i83g2n9ODdKk+Vjilk04SeIhwJ5TO3IAnrs+WDnHaQKBgC5wQI35xrhJKD12SI2P\r\n"
    "VFdX3+5WUEIJ5Sivu5r2a5nPY+Ee2H5NuhWf742VFAg6YCfK1yYuK2v/vJ/haLiZ\r\n"
    "qBZe3F6B0UBfQnzUtVbenNliLj60SbBve3INDgTw2abm0v3Dcx4wbMT8w2ShiC83\r\n"
    "7Rr0nUhGs0IUayU5dLpd3qB1\r\n"
    "-----END PRIVATE KEY-----\r\n";

const char *server_cert =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDyTCCArGgAwIBAgIJAPEyjXMZ6kAsMA0GCSqGSIb3DQEBCwUAMHsxCzAJBgNV\r\n"
    "BAYTAkFVMQwwCgYDVQQIDANWaWMxEjAQBgNVBAcMCU1lbGJvdXJuZTEeMBwGA1UE\r\n"
    "CgwVZXNwLW9wZW4tcnRvcyBQdHkgTHRkMRwwGgYDVQQLDBNTbmFrZW9pbCBTYWxl\r\n"
    "cyBVbml0MQwwCgYDVQQDDANlc3AwHhcNMTYwMjA4MDM0NDU1WhcNMTcwMjA3MDM0\r\n"
    "NDU1WjB7MQswCQYDVQQGEwJBVTEMMAoGA1UECAwDVmljMRIwEAYDVQQHDAlNZWxi\r\n"
    "b3VybmUxHjAcBgNVBAoMFWVzcC1vcGVuLXJ0b3MgUHR5IEx0ZDEcMBoGA1UECwwT\r\n"
    "U25ha2VvaWwgU2FsZXMgVW5pdDEMMAoGA1UEAwwDZXNwMIIBIjANBgkqhkiG9w0B\r\n"
    "AQEFAAOCAQ8AMIIBCgKCAQEAq+T6u1QESSSmuYJevBdkyQEbCe9X6eGuPLlT8MLZ\r\n"
    "uy+4pYvqsYbQe9wDo6Ykf2aiQZKTK0o2zXRz6b8/QZNeuJrL/NQ18w7s5+UF7r2y\r\n"
    "POyBXZwjPiPsS0jwowWz4z0LJu1CVBvaX4wAGZReCgDedzi6Sjnrurre29JRydR/\r\n"
    "2C3gTjHEXk2zm2GukBzuPkZe5/TDJA4NYHgrFmrOi85z1TNScu5IeyGWdwLPuHUP\r\n"
    "ld1Sd/7f/pTiuTvlrQFmfem+wB8XNA5IAi1s/fuH6UcR7Yw+LAO51xKYJOqmKkv/\r\n"
    "DdLRykAQvrg0lsRN7dJhdia9Q9YBpRct1Vea9nxH1je+CQIDAQABo1AwTjAdBgNV\r\n"
    "HQ4EFgQUkg2t3waA/P/HYDEebQS/6NbWryEwHwYDVR0jBBgwFoAUkg2t3waA/P/H\r\n"
    "YDEebQS/6NbWryEwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEApIvY\r\n"
    "FN+ufDkqiGYwZygX4uo2ysAE3QtruwsFnna0+asat/TNlrFivif9pEW50pHnZV2+\r\n"
    "WZte/LgemeSwSqGekVSYGzbeZciuOdrnwadyBqGJoLTB+JjJRJqdlVR+OrVme0J2\r\n"
    "rBZkQ85/LZAZ8XoLDPopg++nU35NBEK5qImEWyUJb1TPUZbVKQu4aPi9ArprDnDd\r\n"
    "62UyNiDWvHx9yR2hDb9oI5jVnMHNLSCrD2baBVktFwPgJ2gdnPXOzhWHaLVKzl+7\r\n"
    "TMjIxLP6uepl7rg0P6j0UCYt7dSTLlb06wi0zWpUVErpP+sGOXYLDLfls+Ewofd9\r\n"
    "MYQhZNPFw+6LzuwWCA==\r\n"
    "-----END CERTIFICATE-----\r\n";
