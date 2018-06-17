/*!
    \ingroup RSA
    
    \brief This function initializes a provided RsaKey struct. It also takes 
    in a heap identifier, for use with user defined memory overrides 
    (see XMALLOC, XFREE, XREALLOC).
    
    \return 0 Returned upon successfully initializing the RSA structure for 
    use with encryption and decryption
    \return BAD_FUNC_ARGS Returned if the RSA key pointer evaluates to NULL
    
    \param key pointer to the RsaKey structure to initialize
    \param heap pointer to a heap identifier, for use with memory overrides, 
    allowing custom handling of memory allocation. This heap will be the 
    default used when allocating memory for use with this RSA object
    
    _Example_
    \code
    RsaKey enc;
    int ret;
    ret = wc_InitRsaKey(&enc, NULL); // not using heap hint. No custom memory
    if ( ret != 0 ) {
    	// error initializing RSA key
    }
    \endcode
    
    \sa wc_RsaInitCavium
    \sa wc_FreeRsaKey
*/
WOLFSSL_API int  wc_InitRsaKey(RsaKey* key, void* heap);

/*!
    \ingroup RSA
    
    \brief This function frees a provided RsaKey struct using mp_clear.
    
    \return 0 Returned upon successfully freeing the key
    
    \param key pointer to the RsaKey structure to free
    
    _Example_
    \code
    RsaKey enc;
    wc_RsaInitKey(&enc, NULL); // not using heap hint. No custom memory
    ... set key, do encryption

    wc_FreeRsaKey(&enc);
    \endcode
    
    \sa wc_InitRsaKey
*/
WOLFSSL_API int  wc_FreeRsaKey(RsaKey* key);

/*!
    \ingroup RSA
    
    \brief This function encrypts a message from in and stores the result 
    in out. It requires an initialized public key and a random number 
    generator. As a side effect, this function will return the bytes written 
    to out in outLen.
    
    \return Success Upon successfully encrypting the input message, returns 
    the number bytes written to out
    \return -1 Returned if there is an error during RSA encryption and 
    hardware acceleration via Cavium is enabled
    \return BAD_FUNC_ARG Returned if any of the input parameters are invalid
    \return RSA_BUFFER_E Returned if the output buffer is too small to store 
    the ciphertext
    \return RNG_FAILURE_E Returned if there is an error generating a random 
    block using the provided RNG structure
    \return MP_INIT_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_READ_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_CMP_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_INVMOD_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_EXPTMOD_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_MOD_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_MUL_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_ADD_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_MULMOD_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_TO_E May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_MEM May be returned if there is an error in the math 
    library used while encrypting the message
    \return MP_ZERO_E May be returned if there is an error in the math 
    library used while encrypting the message
    
    \param in pointer to a buffer containing the input message to encrypt
    \param inLen the length of the message to encrypt
    \param out pointer to the buffer in which to store the output ciphertext
    \param outLen the length of the output buffer
    \param key pointer to the RsaKey structure containing the public 
    key to use for encryption
    \param rng The RNG structure with which to generate random block padding
    
    _Example_
    \code
    RsaKey pub;
    int ret = 0;
    byte n[] = { // initialize with received n component of public key };
    byte e[] = { // initialize with received e component of public key };
    byte msg[] = { // initialize with plaintext of message to encrypt };
    byte cipher[256]; // 256 bytes is large enough to store 2048 bit RSA 
    ciphertext
    
    wc_InitRsaKey(&pub, NULL); // not using heap hint. No custom memory
    wc_RsaPublicKeyDecodeRaw(n, sizeof(n), e, sizeof(e), &pub);
    // initialize with received public key parameters
    ret = wc_RsaPublicEncrypt(msg, sizeof(msg), out, sizeof(out), &pub, &rng);
    if ( ret != 0 ) {
    	// error encrypting message
    }
    \endcode
    
    \sa wc_RsaPrivateDecrypt
*/
WOLFSSL_API int  wc_RsaPublicEncrypt(const byte* in, word32 inLen, byte* out,
                                 word32 outLen, RsaKey* key, WC_RNG* rng);

/*!
    \ingroup RSA
    
    \brief This functions is utilized by the wc_RsaPrivateDecrypt function 
    for decrypting.
    
    \return Success Length of decrypted data.
    \return RSA_PAD_E RsaUnPad error, bad formatting
    
    \param in The byte array to be decrypted.
    \param inLen The length of in.
    \param out The byte array for the decrypted data to be stored.
    \param key The key to use for decryption.
    
    _Example_
    \code
    none
    \endcode
    
    \sa wc_RsaPrivateDecrypt
*/
WOLFSSL_API int  wc_RsaPrivateDecryptInline(byte* in, word32 inLen, byte** out,
                                        RsaKey* key);

/*!
    \ingroup RSA
    
    \brief This functions provides private RSA decryption.
    
    \return Success length of decrypted data.
    \return MEMORY_E -125, out of memory error
    \return BAD_FUNC_ARG -173, Bad function argument provided

    \param in The byte array to be decrypted.
    \param inLen The length of in.
    \param out The byte array for the decrypted data to be stored.
    \param outLen The length of out.
    \param key The key to use for decryption.
    
    _Example_
    \code
    ret = wc_RsaPublicEncrypt(in, inLen, out, sizeof(out), &key, &rng);
    if (ret < 0) {
        return -1;
    }
    ret = wc_RsaPrivateDecrypt(out, ret, plain, sizeof(plain), &key);
    if (ret < 0) {
        return -1;
    }
    \endcode

    \sa RsaUnPad
    \sa wc_RsaFunction
    \sa wc_RsaPrivateDecryptInline
*/
WOLFSSL_API int  wc_RsaPrivateDecrypt(const byte* in, word32 inLen, byte* out,
                                  word32 outLen, RsaKey* key);

/*!
    \ingroup RSA
    
    \brief Signs the provided array with the private key.
    
    \return RSA_BUFFER_E: -131, RSA buffer error, output too small or 
    input too large
    
    \param in The byte array to be encrypted.
    \param inLen The length of in.
    \param out The byte array for the encrypted data to be stored.
    \param outLen The length of out.
    \param key The key to use for encryption.
    \param RNG The RNG struct to use for random number purposes.
    
    _Example_
    \code
    ret = wc_RsaSSL_Sign(in, inLen, out, sizeof(out), &key, &rng);
    if (ret < 0) {
        return -1;
    }
    memset(plain, 0, sizeof(plain));
    ret = wc_RsaSSL_Verify(out, ret, plain, sizeof(plain), &key);
    if (ret < 0) {
        return -1;
    }
    \endcode
    
    \sa wc_RsaPad
*/
WOLFSSL_API int  wc_RsaSSL_Sign(const byte* in, word32 inLen, byte* out,
                            word32 outLen, RsaKey* key, WC_RNG* rng);

/*!
    \ingroup RSA
    
    \brief Used to verify that the message was signed by RSA key.  The output 
    uses the same byte array as the input.
    
    \return >0 Length of text.
    \return <0 An error occurred.
    
    \param in Byte array to be decrypted.
    \param inLen Length of the buffer input.
    \param out Pointer to a pointer for decrypted information.
    \param key RsaKey to use.
    
    _Example_
    \code
    RsaKey key;
    WC_WC_RNG rng;
    int ret = 0;
    long e = 65537; // standard value to use for exponent
    wc_InitRsaKey(&key, NULL); // not using heap hint. No custom memory
    wc_InitRng(&rng);
    wc_MakeRsaKey(&key, 2048, e, &rng);

    byte in[] = { // Initialize with some RSA encrypted information }
    byte* out;
    if(wc_RsaSSL_VerifyInline(in, sizeof(in), &out, &key) < 0)
    {
        // handle error
    }
    \endcode
    
    \sa wc_RsaSSL_Verify
    \sa wc_RsaSSL_Sign
*/
WOLFSSL_API int  wc_RsaSSL_VerifyInline(byte* in, word32 inLen, byte** out,
                                    RsaKey* key);

/*!
    \ingroup RSA
    
    \brief Used to verify that the message was signed by key.
    
    \return Success Length of text on no error.
    \return MEMORY_E memory exception.

    \param in The byte array to be decrypted.
    \param inLen The length of in.
    \param out The byte array for the decrypted data to be stored.
    \param outLen The length of out.
    \param key The key to use for verification.

    _Example_
    \code
    ret = wc_RsaSSL_Sign(in, inLen, out, sizeof(out), &key, &rng);
    if (ret < 0) {
        return -1;
    }
    memset(plain, 0, sizeof(plain));
    ret = wc_RsaSSL_Verify(out, ret, plain, sizeof(plain), &key);
    if (ret < 0) {
        return -1;
    }
    \endcode
    
    \sa wc_RsaSSL_Sign
*/
WOLFSSL_API int  wc_RsaSSL_Verify(const byte* in, word32 inLen, byte* out,
                              word32 outLen, RsaKey* key);

/*!
    \ingroup RSA
    
    \brief Returns the encryption size for the provided key structure.
    
    \return Success Encryption size for the provided key structure.
    
    \param key The key to use for verification.
    
    _Example_
    \code
    int sz = wc_RsaEncryptSize(&key);
    \endcode
    
    \sa wc_InitRsaKey
    \sa wc_InitRsaKey_ex
    \sa wc_MakeRsaKey
    \sa XMEMSET
*/
WOLFSSL_API int  wc_RsaEncryptSize(RsaKey* key);

/*!
    \ingroup RSA
    
    \brief This function parses a DER-formatted RSA private key, extracts the 
    private key and stores it in the given RsaKey structure. It also sets the 
    distance parsed in idx.

    \return 0 Returned upon successfully parsing the private key from the DER 
    encoded input
    \return ASN_PARSE_E Returned if there is an error parsing the private key 
    from the input buffer. This may happen if the input private key is not 
    properly formatted according to ASN.1 standards
    \return ASN_RSA_KEY_E Returned if there is an error reading the private 
    key elements of the RSA key input

    \param input pointer to the buffer containing the DER formatted private 
    key to decode
    \param inOutIdx pointer to the index in the buffer at which the key begins 
    (usually 0). As a side effect of this function, inOutIdx will store the 
    distance parsed through the input buffer
    \param key pointer to the RsaKey structure in which to store the decoded 
    private key
    \param inSz size of the input buffer
    
    _Example_
    \code
    RsaKey enc;
    word32 idx = 0;
    int ret = 0;
    byte der[] = { // initialize with DER-encoded RSA private key };

    wc_InitRsaKey(&enc, NULL); // not using heap hint. No custom memory
    ret = wc_RsaPrivateKeyDecode(der, &idx, &enc, sizeof(der));
    if( ret != 0 ) {
    	// error parsing private key 
    }
    \endcode
    
    \sa wc_RsaPublicKeyDecode
    \sa wc_MakeRsaKey
*/
WOLFSSL_API int  wc_RsaPrivateKeyDecode(const byte* input, word32* inOutIdx,
                                                               RsaKey*, word32);

/*!
    \ingroup RSA
    
    \brief This function parses a DER-formatted RSA public key, extracts the 
    public key and stores it in the given RsaKey structure. It also sets the 
    distance parsed in idx.
    
    \return 0 Returned upon successfully parsing the public key from the DER 
    encoded input
    \return ASN_PARSE_E Returned if there is an error parsing the public key 
    from the input buffer. This may happen if the input public key is not 
    properly formatted according to ASN.1 standards
    \return ASN_OBJECT_ID_E Returned if the ASN.1 Object ID does not match 
    that of a RSA public key
    \return ASN_EXPECT_0_E Returned if the input key is not correctly 
    formatted according to ASN.1 standards
    \return ASN_BITSTR_E Returned if the input key is not correctly formatted 
    according to ASN.1 standards 
    \return ASN_RSA_KEY_E Returned if there is an error reading the public key 
    elements of the RSA key input

    \param input pointer to the buffer containing the input DER-encoded RSA 
    public key to decode
    \param inOutIdx pointer to the index in the buffer at which the key 
    begins (usually 0). As a side effect of this function, inOutIdx will 
    store the distance parsed through the input buffer
    \param key pointer to the RsaKey structure in which to store the decoded 
    public key
    \param inSz size of the input buffer

    _Example_
    \code
    RsaKey pub;
    word32 idx = 0;
    int ret = 0;
    byte der[] = { // initialize with DER-encoded RSA public key };

    wc_InitRsaKey(&pub, NULL); // not using heap hint. No custom memory
    ret = wc_RsaPublicKeyDecode(der, &idx, &pub, sizeof(der));
    if( ret != 0 ) {
    	// error parsing public key 
    }
    \endcode
    
    \sa wc_RsaPublicKeyDecodeRaw
*/
WOLFSSL_API int  wc_RsaPublicKeyDecode(const byte* input, word32* inOutIdx,
                                                               RsaKey*, word32);

/*!
    \ingroup RSA
    
    \brief This function decodes the raw elements of an RSA public key, taking 
    in the public modulus (n) and exponent (e). It stores these raw elements 
    in the provided RsaKey structure, allowing one to use them in the 
    encryption/decryption process.
    
    \return 0 Returned upon successfully decoding the raw elements of the 
    public key into the RsaKey structure
    \return BAD_FUNC_ARG Returned if any of the input arguments evaluates to 
    NULL
    \return MP_INIT_E Returned if there is an error initializing an integer 
    for use with the multiple precision integer (mp_int) library
    \return ASN_GETINT_E Returned if there is an error reading one of the 
    provided RSA key elements, n or e

    \param n pointer to a buffer containing the raw modulus parameter of the 
    public RSA key
    \param nSz size of the buffer containing n
    \param e pointer to a buffer containing the raw exponent parameter of 
    the public RSA key
    \param eSz size of the buffer containing e
    \param key pointer to the RsaKey struct to initialize with the provided 
    public key elements

    _Example_
    \code
    RsaKey pub;
    int ret = 0;
    byte n[] = { // initialize with received n component of public key };
    byte e[] = { // initialize with received e component of public key };

    wc_InitRsaKey(&pub, NULL); // not using heap hint. No custom memory
    ret = wc_RsaPublicKeyDecodeRaw(n, sizeof(n), e, sizeof(e), &pub);
    if( ret != 0 ) {
    	// error parsing public key elements
    }
    \endcode
    
    \sa wc_RsaPublicKeyDecode
*/
WOLFSSL_API int  wc_RsaPublicKeyDecodeRaw(const byte* n, word32 nSz,
                                        const byte* e, word32 eSz, RsaKey* key);

/*!
    \ingroup RSA
    
    \brief This function converts an RsaKey key to DER format.  The result is 
    written to output and it returns the number of bytes written.
    
    \return 0 Success
    \return BAD_FUNC_ARG Returned if key or output is null, or if key->type 
    is not RSA_PRIVATE, or if inLen isn't large enough for output buffer.
    \return MEMORY_E Returned if there is an error allocating memory.
    
    \param key Initialized RsaKey structure.
    \param output Pointer to output buffer.
    \param inLen Size of output buffer.
    
    _Example_
    \code
    byte* der;
    // Allocate memory for der
    int derSz = // Amount of memory allocated for der;
    RsaKey key;
    WC_WC_RNG rng;
    long e = 65537; // standard value to use for exponent
    ret = wc_MakeRsaKey(&key, 2048, e, &rng); // generate 2048 bit long 
    private key
    wc_InitRsaKey(&key, NULL);
    wc_InitRng(&rng);
    if(wc_RsaKeyToDer(&key, der, derSz) != 0)
    {
        // Handle the error thrown
    }
    \endcode
    
    \sa wc_RsaKeyToPublicDer
    \sa wc_InitRsaKey
    \sa wc_MakeRsaKey
    \sa wc_InitRng
*/
    WOLFSSL_API int wc_RsaKeyToDer(RsaKey*, byte* output, word32 inLen);

/*!
    \ingroup RSA
    
    \brief This function performs RSA encrypt while allowing the choice of 
    which padding to use.
    
    \return size On successfully encryption the size of the encrypted buffer 
    is returned
    \return RSA_BUFFER_E RSA buffer error, output too small or input too large
    
    \param in pointer to the buffer for encryption
    \param inLen length of the buffer to encrypt
    \param out encrypted msg created
    \param outLen length of buffer available to hold encrypted msg
    \param key initialized RSA key struct
    \param rng initialized WC_RNG struct
    \param type type of padding to use (WC_RSA_OAEP_PAD or WC_RSA_PKCSV15_PAD)
    \param hash type of hash to use (choices can be found in hash.h)
    \param mgf type of mask generation function to use
    \param label an optional label to associate with encrypted message
    \param labelSz size of the optional label used

    _Example_
    \code
    WC_WC_WC_RNG rng;
    RsaKey key;
    byte in[] = “I use Turing Machines to ask questions”
    byte out[256];
    int ret;
    …

    ret = wc_RsaPublicEncrypt_ex(in, sizeof(in), out, sizeof(out), &key, &rng, 
    WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    if (ret < 0) {
	    //handle error
    }
    \endcode
    
    \sa wc_RsaPublicEncrypt
    \sa wc_RsaPrivateDecrypt_ex
*/
WOLFSSL_API int  wc_RsaPublicEncrypt_ex(const byte* in, word32 inLen, byte* out,
                   word32 outLen, RsaKey* key, WC_RNG* rng, int type,
                   enum wc_HashType hash, int mgf, byte* label, word32 lableSz);

/*!
    \ingroup RSA
    
    \brief This function uses RSA to decrypt a message and gives the 
    option of what padding type.
    
    \return size On successful decryption, the size of the decrypted message 
    is returned.
    \return MEMORY_E Returned if not enough memory on system to malloc a 
    needed array.
    \return BAD_FUNC_ARG Returned if a bad argument was passed into the 
    function.
    
    \param in pointer to the buffer for decryption
    \param inLen length of the buffer to decrypt
    \param out decrypted msg created
    \param outLen length of buffer available to hold decrypted msg
    \param key initialized RSA key struct
    \param type type of padding to use (WC_RSA_OAEP_PAD or WC_RSA_PKCSV15_PAD)
    \param hash type of hash to use (choices can be found in hash.h)
    \param mgf type of mask generation function to use
    \param label an optional label to associate with encrypted message
    \param labelSz size of the optional label used

    _Example_
    \code
    WC_WC_WC_RNG rng;
    RsaKey key;
    byte in[] = “I use Turing Machines to ask questions”
    byte out[256];
    byte plain[256];
    int ret;
    …
    ret = wc_RsaPublicEncrypt_ex(in, sizeof(in), out, sizeof(out), &key, 
    &rng, WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    if (ret < 0) {
	    //handle error
    }
    …
    ret = wc_RsaPrivateDecrypt_ex(out, ret, plain, sizeof(plain), &key, 
    WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
    
    if (ret < 0) {
	    //handle error
    }
    \endcode
    
    \sa none
*/
WOLFSSL_API int  wc_RsaPrivateDecrypt_ex(const byte* in, word32 inLen,
                   byte* out, word32 outLen, RsaKey* key, int type,
                   enum wc_HashType hash, int mgf, byte* label, word32 lableSz);

/*!
    \ingroup RSA
    
    \brief This function uses RSA to decrypt a message inline and gives the 
    option of what padding type. The in buffer will contain the decrypted 
    message after being called and the out byte pointer will point to the 
    location in the “in” buffer where the plain text is.
    
    \return size On successful decryption, the size of the decrypted message 
    is returned.
    \return MEMORY_E: Returned if not enough memory on system to malloc a 
    needed array.
    \return RSA_PAD_E: Returned if an error in the padding was encountered.
    \return BAD_PADDING_E: Returned if an error happened during parsing past 
    padding.
    \return BAD_FUNC_ARG: Returned if a bad argument was passed into the 
    function.

    \param in pointer to the buffer for decryption
    \param inLen length of the buffer to decrypt
    \param out pointer to location of decrypted message in “in” buffer
    \param key initialized RSA key struct
    \param type type of padding to use (WC_RSA_OAEP_PAD or WC_RSA_PKCSV15_PAD)
    \param hash type of hash to use (choices can be found in hash.h)
    \param mgf type of mask generation function to use
    \param label an optional label to associate with encrypted message
    \param labelSz size of the optional label used

    _Example_
    \code
    WC_WC_WC_RNG rng;
    RsaKey key;
    byte in[] = “I use Turing Machines to ask questions”
    byte out[256];
    byte* plain;
    int ret;
    …
    ret = wc_RsaPublicEncrypt_ex(in, sizeof(in), out, sizeof(out), &key, 
    &rng, WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);

    if (ret < 0) {
	    //handle error
    }
    …
    ret = wc_RsaPrivateDecryptInline_ex(out, ret, &plain, &key, 
    WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);

    if (ret < 0) {
	    //handle error
    }
    \endcode
    
    \sa none
*/
WOLFSSL_API int  wc_RsaPrivateDecryptInline_ex(byte* in, word32 inLen,
                      byte** out, RsaKey* key, int type, enum wc_HashType hash,
                      int mgf, byte* label, word32 lableSz);

/*!
    \ingroup RSA
    
    \brief Flattens the RsaKey structure into individual elements (e, n) 
    used for the RSA algorithm.
    
    \return 0 Returned if the function executed normally, without error.
    \return BAD_FUNC_ARG: Returned if any of the parameters are passed in 
    with a null value.
    \return RSA_BUFFER_E: Returned if the e or n buffers passed in are not 
    the correct size.
    \return MP_MEM: Returned if an internal function has memory errors.
    \return MP_VAL: Returned if an internal function argument is not valid.
    
    \param key The key to use for verification.
    \param e a buffer for the value of e. e is a large positive integer in 
    the RSA modular arithmetic operation.
    \param eSz the size of the e buffer.
    \param n a buffer for the value of n. n is a large positive integer in 
    the RSA modular arithmetic operation.
    \param nSz the size of the n buffer.

    _Example_
    \code
    Rsa key; // A valid RSA key.
    byte e[ buffer sz E.g. 256 ];
    byte n[256];
    int ret;
    word32 eSz = sizeof(e);
    word32 nSz = sizeof(n);
    ...
    ret = wc_RsaFlattenPublicKey(&key, e, &eSz, n, &nSz);
    if (ret != 0) {
    	// Failure case.
    }
    \endcode
    
    \sa wc_InitRsaKey
    \sa wc_InitRsaKey_ex
    \sa wc_MakeRsaKey
    \sa XMEMSET
*/
WOLFSSL_API int  wc_RsaFlattenPublicKey(RsaKey*, byte*, word32*, byte*,
                                                                       word32*);

/*!
    \ingroup RSA
    
    \brief Convert Rsa Public key to DER format.  Writes to output, and 
    returns count of bytes written.
    
    \return >0 Success, number of bytes written.
    \return BAD_FUNC_ARG Returned if key or output is null.
    \return MEMORY_E Returned when an error allocating memory occurs.
    \return <0 Error

    \param key The RSA key structure to convert.
    \param output Output buffer to hold DER.
    \param inLen Length of buffer.
    
    _Example_
    \code
    RsaKey key;

    wc_RsaInitKey(&key, NULL);
    // Use key

    int BUFFER_SIZE = // Some adequate size for the buffer
    byte output[BUFFER_SIZE];
    if(wc_RsaKeyToPublicDer(&key, output, sizeof(output)) != 0)
    {
        // Handle Error
    }
    \endcode
    
    \sa wc_RsaKeyToPublicDer
    \sa wc_RsaInitKey
*/
    WOLFSSL_API int wc_RsaKeyToPublicDer(RsaKey*, byte* output, word32 inLen);

/*!
    \ingroup RSA
    
    \brief This function generates a RSA private key of length size (in bits) 
    and given exponent (e). It then stores this key in the provided RsaKey 
    structure, so that it may be used for encryption/decryption. A secure 
    number to use for e is 65537. size is required to be greater than 
    RSA_MIN_SIZE and less than RSA_MAX_SIZE. For this function to be 
    available, the option WOLFSSL_KEY_GEN must be enabled at compile time. 
    This can be accomplished with --enable-keygen if using ./configure.

    \return 0 Returned upon successfully generating a RSA private key
    \return BAD_FUNC_ARG Returned if any of the input arguments are NULL, 
    the size parameter falls outside of the necessary bounds, or e is 
    incorrectly chosen
    \return RNG_FAILURE_E Returned if there is an error generating a random 
    block using the provided RNG structure
    \return MP_INIT_E
    \return MP_READ_E May be May be returned if there is an error in the math 
    library used while generating the RSA key returned if there is an error 
    in the math library used while generating the RSA key
    \return MP_CMP_E May be returned if there is an error in the math library 
    used while generating the RSA key
    \return MP_INVMOD_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_EXPTMOD_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_MOD_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_MUL_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_ADD_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_MULMOD_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_TO_E May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_MEM May be returned if there is an error in the math 
    library used while generating the RSA key
    \return MP_ZERO_E May be returned if there is an error in the math 
    library used while generating the RSA key
    
    \param key pointer to the RsaKey structure in which to store the 
    generated private key
    \param size desired keylenth, in bits. Required to be greater than 
    RSA_MIN_SIZE and less than RSA_MAX_SIZE
    \param e exponent parameter to use for generating the key. A secure 
    choice is 65537
    \param rng pointer to an RNG structure to use for random number generation 
    while making the ke

    _Example_
    \code
    RsaKey priv;
    WC_WC_RNG rng;
    int ret = 0;
    long e = 65537; // standard value to use for exponent

    wc_InitRsaKey(&priv, NULL); // not using heap hint. No custom memory
    wc_InitRng(&rng);
    // generate 2048 bit long private key
    ret = wc_MakeRsaKey(&priv, 2048, e, &rng); 
    if( ret != 0 ) {
	    // error generating private key
    }
    \endcode
    
    \sa none
*/
    WOLFSSL_API int wc_MakeRsaKey(RsaKey* key, int size, long e, WC_RNG* rng);
