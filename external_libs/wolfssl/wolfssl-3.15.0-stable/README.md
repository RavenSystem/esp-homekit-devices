# Resources

[wolfSSL Website](https://www.wolfssl.com/)

[wolfSSL Wiki](https://github.com/wolfSSL/wolfssl/wiki)

[FIPS FAQ](https://www.wolfssl.com/wolfSSL/fips.html)

[wolfSSL Manual](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-toc.html)

[wolfSSL API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-17-wolfssl-api-reference.html)

[wolfCrypt API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-18-wolfcrypt-api-reference.html)

[TLS 1.3](https://www.wolfssl.com/docs/tls13/)


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

# wolfSSL Release 3.15.0 (05/05/2018)

Release 3.15.0 of wolfSSL embedded TLS has bug fixes and new features including:

* Support for TLS 1.3 Draft versions 23, 26 and 28.
* Add FIPS SGX support!
* Single Precision assembly code added for ARM and 64-bit ARM to enhance performance.
* Improved performance for Single Precision maths on 32-bit.
* Improved downgrade support for the TLS 1.3 handshake.
* Improved TLS 1.3 support from interoperability testing.
* Added option to allow TLS 1.2 to be compiled out to reduce size and enhance security.
* Added option to support Ed25519 in TLS 1.2 and 1.3.
* Update wolfSSL_HMAC_Final() so the length parameter is optional.
* Various fixes for Coverity static analysis reports.
* Add define to use internal struct timeval (USE_WOLF_TIMEVAL_T).
* Switch LowResTimer() to call XTIME instead of time(0) for better portability.
* Expanded OpenSSL compatibility layer with a bevy of new functions.
* Added Renesas CS+ project files.
* Align DH support with NIST SP 800-56A, add wc_DhSetKey_ex() for q parameter.
* Add build option for CAVP self test build (--enable-selftest).
* Expose mp_toradix() when WOLFSSL_PUBLIC_MP is defined.
* Example certificate expiration dates and generation script updated.
* Additional optimizations to trim out unused strings depending on build options.
* Fix for DN tag strings to have “=” when returning the string value to users.
* Fix for wolfSSL_ERR_get_error_line_data return value if no more errors are in the queue.
* Fix for AES-CBC IV value with PIC32 hardware acceleration.
* Fix for wolfSSL_X509_print with ECC certificates.
* Fix for strict checking on URI absolute vs relative path.
* Added crypto device framework to handle PK RSA/ECC operations using callbacks, which adds new build option `./configure --enable-cryptodev` or `WOLF_CRYPTO_DEV`.
* Added devId support to ECC and PKCS7 for hardware based private key.
* Fixes in PKCS7 for handling possible memory leak in some error cases.
* Added test for invalid cert common name when set with `wolfSSL_check_domain_name`.
* Refactor of the cipher suite names to use single array, which contains internal name, IANA name and cipher suite bytes.
* Added new function `wolfSSL_get_cipher_name_from_suite` for getting IANA cipher suite name using bytes.
* Fixes for fsanitize reports.
* Fix for openssl compatibility function `wolfSSL_RSA_verify` to check returned size.
* Fixes and improvements for FreeRTOS AWS.
* Fixes for building openssl compatibility with FreeRTOS.
* Fix and new test for handling match on domain name that may have a null terminator inside.
* Cleanup of the socket close code used for examples, CRL/OCSP and BIO to use single macro `CloseSocket`.
* Refactor of the TLSX code to support returning error codes.
* Added new signature wrapper functions `wc_SignatureVerifyHash` and `wc_SignatureGenerateHash` to allow direct use of hash.
* Improvement to GCC-ARM IDE example.
* Enhancements and cleanups for the ASN date/time code including new API's `wc_GetDateInfo`, `wc_GetCertDates` and `wc_GetDateAsCalendarTime`.
* Fixes to resolve issues with C99 compliance. Added build option `WOLF_C99` to force C99.
* Added a new `--enable-opensslall` option to enable all openssl compatibility features.
* Added new `--enable-webclient` option for enabling a few HTTP API's.
* Added new `wc_OidGetHash` API for getting the hash type from a hash OID.
* Moved `wolfSSL_CertPemToDer`, `wolfSSL_KeyPemToDer`, `wolfSSL_PubKeyPemToDer` to asn.c and renamed to `wc_`. Added backwards compatibility macro for old function names.
* Added new `WC_MAX_SYM_KEY_SIZE` macro for helping determine max key size.
* Added `--enable-enckeys` or (`WOLFSSL_ENCRYPTED_KEYS`) to enable support for encrypted PEM private keys using password callback without having to use opensslextra.
* Added ForceZero on the password buffer after done using it.
* Refactor unique hash types to use same internal values (ex WC_MD5 == WC_HASH_TYPE_MD5).
* Refactor the Sha3 types to use `wc_` naming, while retaining old names for compatibility.
* Improvements to `wc_PBKDF1` to support more hash types and the non-standard extra data option.
* Fix TLS 1.3 with ECC disabled and CURVE25519 enabled.
* Added new define `NO_DEV_URANDOM` to disable the use of `/dev/urandom`.
* Added `WC_RNG_BLOCKING` to indicate block w/sleep(0) is okay.
* Fix for `HAVE_EXT_CACHE` callbacks not being available without `OPENSSL_EXTRA` defined.
* Fix for ECC max bits `MAX_ECC_BITS` not always calculating correctly due to macro order.
* Added support for building and using PKCS7 without RSA (assuming ECC is enabled).
* Fixes and additions for Cavium Nitrox V to support ECC, AES-GCM and HMAC (SHA-224 and SHA3).
* Enabled ECC, AES-GCM and SHA-512/384 by default in (Linux and Windows)
* Added `./configure --enable-base16` and `WOLFSSL_BASE16` configuration option to enable Base16 API's.
* Improvements to ATECC508A support for building without `WOLFSSL_ATMEL` defined.
* Refactor IO callback function names to use `_CTX_` to eliminate confusion about the first parameter.
* Added support for not loading a private key for server or client when `HAVE_PK_CALLBACK` is defined and the private PK callback is set.
* Added new ECC API `wc_ecc_sig_size_calc` to return max signature size for a key size.
* Cleanup ECC point import/export code and added new API `wc_ecc_import_unsigned`.
* Fixes for handling OCSP with non-blocking.
* Added new PK (Primary Key) callbacks for the VerifyRsaSign. The new callbacks API's are `wolfSSL_CTX_SetRsaVerifySignCb` and `wolfSSL_CTX_SetRsaPssVerifySignCb`.
* Added new ECC API `wc_ecc_rs_raw_to_sig` to take raw unsigned R and S and encodes them into ECDSA signature format.
* Added support for `WOLFSSL_STM32F1`.
* Cleanup of the ASN X509 header/footer and XSTRNCPY logic.
* Add copyright notice to autoconf files. (Thanks Brian Aker!)
* Updated the M4 files for autotools. (Thanks Brian Aker!)
* Add support for the cipher suite TLS_DH_anon_WITH_AES256_GCM_SHA384 with test cases. (Thanks Thivya Ashok!)
* Add the TLS alert message unknown_psk_identity (115) from RFC 4279, section 2. (Thanks Thivya Ashok!)
* Fix the case when using TCP with timeouts with TLS. wolfSSL shall be agnostic to network socket behavior for TLS. (DTLS is another matter.) The functions `wolfSSL_set_using_nonblock()` and `wolfSSL_get_using_nonblock()` are deprecated.
* Hush the AR warning when building the static library with autotools.
* Hush the “-pthread” warning when building in some environments.
* Added a dist-hook target to the Makefile to reset the default options.h file.
* Removed the need for the darwin-clang.m4 file with the updates provided by Brian A.
* Renamed the AES assembly file so GCC on the Mac will build it using the preprocessor.
* Add a disable option (--disable-optflags) to turn off the default optimization flags so user may supply their own custom flags.
* Correctly touch the dummy fips.h header.

If you have questions on any of this, then email us at info@wolfssl.com.
See INSTALL file for build instructions.
More info can be found on-line at http://wolfssl.com/wolfSSL/Docs.html
