# Description

The wolfSSL embedded SSL library (formerly CyaSSL) is a lightweight SSL/TLS library written in ANSI C and targeted for embedded, RTOS, and resource-constrained environments - primarily because of its small size, speed, and feature set.  It is commonly used in standard operating environments as well because of its royalty-free pricing and excellent cross platform support.  wolfSSL supports industry standards up to the current TLS 1.3 and DTLS 1.3 levels, is up to 20 times smaller than OpenSSL, and offers progressive ciphers such as ChaCha20, Curve25519, NTRU, and Blake2b.  User benchmarking and feedback reports dramatically better performance when using wolfSSL over OpenSSL.

wolfSSL is powered by the wolfCrypt library. A version of the wolfCrypt cryptography library has been FIPS 140-2 validated (Certificate #2425). For additional information, visit the [wolfCrypt FIPS FAQ](https://www.wolfssl.com/license/fips/) or contact fips@wolfssl.com

## Why Choose wolfSSL?
There are many reasons to choose wolfSSL as your embedded SSL solution. Some of the top reasons include size (typical footprint sizes range from 20-100 kB), support for the newest standards (SSL 3.0, TLS 1.0, TLS 1.1, TLS 1.2, TLS 1.3, DTLS 1.0, and DTLS 1.2), current and progressive cipher support (including stream ciphers), multi-platform, royalty free, and an OpenSSL compatibility API to ease porting into existing applications which have previously used the OpenSSL package. For a complete feature list, see [Section 4.1.](https://www.wolfssl.com/docs/wolfssl-manual/ch4/)

***

# Notes - Please read

## Note 1
```
wolfSSL as of 3.6.6 no longer enables SSLv3 by default.  wolfSSL also no
longer supports static key cipher suites with PSK, RSA, or ECDH.  This means
if you plan to use TLS cipher suites you must enable DH (DH is on by default),
or enable ECC (ECC is on by default), or you must enable static
key cipher suites with
    WOLFSSL_STATIC_DH
    WOLFSSL_STATIC_RSA
    or
    WOLFSSL_STATIC_PSK

though static key cipher suites are deprecated and will be removed from future
versions of TLS.  They also lower your security by removing PFS.  Since current
NTRU suites available do not use ephemeral keys, WOLFSSL_STATIC_RSA needs to be
used in order to build with NTRU suites.


When compiling ssl.c, wolfSSL will now issue a compiler error if no cipher suites
are available.  You can remove this error by defining WOLFSSL_ALLOW_NO_SUITES
in the event that you desire that, i.e., you're not using TLS cipher suites.
```

## Note 2
```

wolfSSL takes a different approach to certificate verification than OpenSSL
does.  The default policy for the client is to verify the server, this means
that if you don't load CAs to verify the server you'll get a connect error,
no signer error to confirm failure (-188).  If you want to mimic OpenSSL
behavior of having SSL_connect succeed even if verifying the server fails and
reducing security you can do this by calling:

wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);

before calling wolfSSL_new();  Though it's not recommended.
```

## Note 3
```
The enum values SHA, SHA256, SHA384, SHA512 are no longer available when
wolfSSL is built with --enable-opensslextra (OPENSSL_EXTRA) or with the macro
NO_OLD_SHA_NAMES. These names get mapped to the OpenSSL API for a single call
hash function. Instead the name WC_SHA, WC_SHA256, WC_SHA384 and WC_SHA512
should be used for the enum name.
```

# wolfSSL Release 3.15.3 (6/20/2018)

Release 3.15.3 of wolfSSL embedded TLS has bug fixes and new features including:

* ECDSA blinding added for hardening against side channel attacks
* Fix for compatibility layer build with no server and no client defined
* Use of optimized Intel assembly instructions on compatible AMD processor
* wolfCrypt Nucleus port additions
* Fix added for MatchDomainName and additional tests added
* Fixes for building with ‘WOLFSSL_ATECC508A’ defined
* Fix for verifying a PKCS7 file in BER format with indefinite size


This release of wolfSSL fixes 2 security vulnerability fixes.

Medium level fix for PRIME + PROBE attack combined with a variant of Lucky 13. Constant time hardening was done to avoid potential cache-based side channel attacks when verifying the MAC on a TLS packet. CBC cipher suites are susceptible on systems where an attacker could gain access and run a parallel program for inspecting caching. Only wolfSSL users that are using TLS/DTLS CBC cipher suites need to update. Users that have only AEAD and stream cipher suites set, or have built with WOLFSSL_MAX_STRENGTH (--enable-maxstrength), are not vulnerable. Thanks to Eyal Ronen, Kenny Paterson, and Adi Shamir for the report.

Medium level fix for a ECDSA side channel attack. wolfSSL is one of over a dozen vendors mentioned in the recent Technical Advisory “ROHNP” by author Ryan Keegan. Only wolfSSL users with long term ECDSA private keys using our fastmath or normal math libraries on systems where attackers can get access to the machine using the ECDSA key need to update.  An attacker gaining access to the system could mount a memory cache side channel attack that could recover the key within a few thousand signatures. wolfSSL users that are not using ECDSA private keys, that are using the single precision math library, or that are using ECDSA offloading do not need to update. (blog with more information https://www.wolfssl.com/wolfssh-and-rohnp/)


See INSTALL file for build instructions.
More info can be found on-line at http://wolfssl.com/wolfSSL/Docs.html

# Resources

[wolfSSL Website](https://www.wolfssl.com/)

[wolfSSL Wiki](https://github.com/wolfSSL/wolfssl/wiki)

[FIPS FAQ](https://www.wolfssl.com/wolfSSL/fips.html)

[wolfSSL Manual](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-toc.html)

[wolfSSL API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-17-wolfssl-api-reference.html)

[wolfCrypt API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-18-wolfcrypt-api-reference.html)

[TLS 1.3](https://www.wolfssl.com/docs/tls13/)
