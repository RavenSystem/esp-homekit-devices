/*!
    \ingroup ED25519
    
    \brief This function generates a new ed25519_key and stores it in key.
    
    \retrun 0 Returned upon successfully making an ed25519_key
    \retrun BAD_FUNC_ARG Returned if rng or key evaluate to NULL, or if the 
    specified key size is not 32 bytes (ed25519 has 32 byte keys)
    \retrun MEMORY_E Returned if there is an error allocating memory 
    during function execution

    \param rng pointer to an initialized RNG object with which to 
    generate the key
    \param keysize length of key to generate. Should always be 32 for ed25519
    \param key pointer to the ed25519_key for which to generate a key

    _Example_
    \code
    ed25519_key key;
    wc_ed25519_init(&key);
    WC_RNG rng;
    wc_InitRng(&rng);
    wc_ed25519_make_key(&rng, 32, &key); // initialize 32 byte ed25519 key
    \endcode
    
    \sa wc_ed25519_init
*/
WOLFSSL_API
int wc_ed25519_make_key(WC_RNG* rng, int keysize, ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function signs a message digest using an ed25519_key object 
    to guarantee authenticity.
    
    \return 0 Returned upon successfully generating a signature for the 
    message digest
    \return BAD_FUNC_ARG Returned any of the input parameters evaluate to 
    NULL, or if the output buffer is too small to store the generated signature
    \return MEMORY_E Returned if there is an error allocating memory during 
    function execution
    
    \param in pointer to the buffer containing the message to sign
    \param inlen length of the message to sign
    \param out buffer in which to store the generated signature
    \param outlen max length of the output buffer. Will store the bytes 
    written to out upon successfully generating a message signature
    \param key pointer to a private ed25519_key with which to generate the 
    signature
    
    _Example_
    \code
    ed25519_key key;
    WC_RNG rng;
    int ret, sigSz;

    byte sig[64]; // will hold generated signature
    sigSz = sizeof(sig);
    byte message[] = { // initialize with message };

    wc_InitRng(&rng); // initialize rng
    wc_ed25519_init(&key); // initialize key
    wc_ed25519_make_key(&rng, 32, &key); // make public/private key pair
    ret = wc_ed25519_sign_msg(message, sizeof(message), sig, &sigSz, &key);
    if ( ret != 0 ) {
    	// error generating message signature
    }
    \endcode
    
    \sa wc_ed25519_verify_msg
*/
WOLFSSL_API
int wc_ed25519_sign_msg(const byte* in, word32 inlen, byte* out,
                        word32 *outlen, ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function verifies the ed25519 signature of a message to ensure 
    authenticity. It returns the answer through stat, with 1 corresponding to 
    a valid signature, and 0 corresponding to an invalid signature.
    
    \return 0 Returned upon successfully performing the signature 
    verification. Note: This does not mean that the signature is verified. 
    The authenticity information is stored instead in stat
    \return BAD_FUNC_ARG Returned if any of the input parameters evaluate to 
    NULL, or if the siglen does not match the actual length of a signature
    \return 1 Returned if verification completes, but the signature generated 
    does not match the signature provided
    
     \param sig pointer to the buffer containing the signature to verify
     \param siglen length of the signature to verify
     \param msg pointer to the buffer containing the message to verify
     \param msglen length of the message to verify
     \param stat pointer to the result of the verification. 1 indicates the 
    message was successfully verified
     \param key pointer to a public ed25519 key with which to verify the 
    signature

    _Example_
    \code
    ed25519_key key;
    int ret, verified = 0;

    byte sig[] { // initialize with received signature };
    byte msg[] = { // initialize with message };
    // initialize key with received public key
    ret = wc_ed25519_verify_msg(sig, sizeof(sig), msg, sizeof(msg), 
    &verified, &key);

    if ( return < 0 ) {
	    // error performing verification
    } else if ( verified == 0  )
	    // the signature is invalid
    }
    \endcode
    
    \sa wc_ed25519_sign_msg
*/
WOLFSSL_API
int wc_ed25519_verify_msg(const byte* sig, word32 siglen, const byte* msg,
                          word32 msglen, int* stat, ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function initializes an ed25519_key object for future use 
    with message verification.
    
    \return 0 Returned upon successfully initializing the ed25519_key object
    \return BAD_FUNC_ARG Returned if key is NULL
    
    \param key pointer to the ed25519_key object to initialize
    
    _Example_
    \code
    ed25519_key key;
    wc_ed25519_init(&key);
    \endcode
    
    \sa wc_ed25519_make_key
    \sa wc_ed25519_free
*/
WOLFSSL_API
int wc_ed25519_init(ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function frees an ed25519 object after it has been used.
    
    \return none No returns.
    
    \param key pointer to the ed25519_key object to free
    
    _Example_
    \code
    ed25519_key key;
    // initialize key and perform secure exchanges
    ...
    wc_ed25519_free(&key);
    \endcode
    
    \sa wc_ed25519_init
*/
WOLFSSL_API
void wc_ed25519_free(ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function imports a public ed25519_key pair from a buffer 
    containing the public key. This function will handle both compressed and 
    uncompressed keys.
    
    \return 0 Returned on successfully importing the ed25519_key
    \return BAD_FUNC_ARG Returned if in or key evaluate to NULL, or inLen is 
    less than the size of an ed25519 key
    
    \param in pointer to the buffer containing the public key
    \param inLen length of the buffer containing the public key
    \param key pointer to the ed25519_key object in which to store the 
    public key
    
    _Example_
    \code
    int ret;
    byte pub[] = { // initialize ed25519 public key };

    ed_25519 key;
    wc_ed25519_init_key(&key);
    ret = wc_ed25519_import_public(pub, sizeof(pub), &key);
    if ( ret != 0) {
    	// error importing key
    }
    \endcode
    
    \sa wc_ed25519_import_private_key
    \sa wc_ed25519_export_public
*/
WOLFSSL_API
int wc_ed25519_import_public(const byte* in, word32 inLen, ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function imports a public/private ed25519 key pair from a 
    pair of buffers. This function will handle both compressed and 
    uncompressed keys.
    
    \return 0 Returned on successfully importing the ed25519_key
    \return BAD_FUNC_ARG Returned if in or key evaluate to NULL, or if 
    either privSz or pubSz are less than the size of an ed25519 key
    
    \param priv pointer to the buffer containing the private key
    \param privSz size of the private key
    \param pub pointer to the buffer containing the public key
    \param pubSz length of the public key
    \param key pointer to the ed25519_key object in which to store the 
    imported private/public key pair

    _Example_
    \code
    int ret;
    byte priv[] = { // initialize with 32 byte private key };
    byte pub[]  = { // initialize with the corresponding public key };

    ed25519_key key;
    wc_ed25519_init_key(&key);
    ret = wc_ed25519_import_private_key(priv, sizeof(priv), pub, 
    sizeof(pub), &key);
    if ( ret != 0) {
    	// error importing key
    }
    \endcode
    
    \sa wc_ed25519_import_public_key
    \sa wc_ed25519_export_private_only
*/
WOLFSSL_API
int wc_ed25519_import_private_key(const byte* priv, word32 privSz,
                               const byte* pub, word32 pubSz, ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function exports the private key from an ed25519_key 
    structure. It stores the public key in the buffer out, and sets the bytes 
    written to this buffer in outLen.
    
    \return 0 Returned upon successfully exporting the public key
    \return BAD_FUNC_ARG Returned if any of the input values evaluate to NULL
    \return BUFFER_E Returned if the buffer provided is not large enough to 
    store the private key. Upon returning this error, the function sets the 
    size required in outLen
    
    \param key pointer to an ed25519_key structure from which to export the 
    public key
    \param out pointer to the buffer in which to store the public key
    \param outLen pointer to a word32 object with the size available in out. 
    Set with the number of bytes written to out after successfully exporting 
    the private key

    _Example_
    \code
    int ret;
    ed25519_key key;
    // initialize key, make key

    char pub[32];
    word32 pubSz = sizeof(pub);

    ret = wc_ed25519_export_public(&key, pub, &pubSz);
    if ( ret != 0) {
	    // error exporting public key
    }
    \endcode
    
    \sa wc_ed25519_import_public_key
    \sa wc_ed25519_export_private_only
*/
WOLFSSL_API
int wc_ed25519_export_public(ed25519_key*, byte* out, word32* outLen);
/*!
    \ingroup ED25519
    
    \brief This function exports only the private key from an ed25519_key 
    structure. It stores the private key in the buffer out, and sets 
    the bytes written to this buffer in outLen.
    
    \return 0 Returned upon successfully exporting the private key
    \return ECC_BAD_ARG_E Returned if any of the input values evaluate to NULL
    \return BUFFER_E Returned if the buffer provided is not large enough 
    to store the private key
    
    \param key pointer to an ed25519_key structure from which to export 
    the private key
    \param out pointer to the buffer in which to store the private key
    \param outLen pointer to a word32 object with the size available in 
    out. Set with the number of bytes written to out after successfully 
    exporting the private key
    
    _Example_
    \code
    int ret;
    ed25519_key key;
    // initialize key, make key

    char priv[32]; // 32 bytes because only private key
    word32 privSz = sizeof(priv);
    ret = wc_ed25519_export_private_only(&key, priv, &privSz);
    if ( ret != 0) {
    	// error exporting private key
    }
    \endcode
    
    \sa wc_ed25519_export_public
    \sa wc_ed25519_import_private_key
*/
WOLFSSL_API
int wc_ed25519_export_private_only(ed25519_key* key, byte* out, word32* outLen);
/*!
    \ingroup ED25519
    
    \brief Export the private key, including public part.

    \return 0 Success
    \return BAD_FUNC_ARG Returns if any argument is null.
    \return BUFFER_E Returns if outLen is less than ED25519_PRV_KEY_SIZE

    \param key ed25519_key struct to export from.
    \param out Destination for private key.
    \param outLen Max length of output, set to the length of the exported 
    private key.
    
    _Example_
    \code
    ed25519_key key;
    wc_ed25519_init(&key);

    WC_RNG rng;
    wc_InitRng(&rng);

    wc_ed25519_make_key(&rng, 32, &key); // initialize 32 byte ed25519 key

    byte out[32]; // out needs to be a sufficient buffer size
    word32 outLen = sizeof(out);
    int key_size = wc_ed25519_export_private(&key, out, &outLen);
    if(key_size == BUFFER_E)
    {
        // Check size of out compared to outLen to see if function reset outLen
    }
    \endcode
    
    \sa none
*/
WOLFSSL_API
int wc_ed25519_export_private(ed25519_key* key, byte* out, word32* outLen);
/*!
    \ingroup ED25519
    
    \brief Export full private key and public key.
    
    \return 0 Success
    \return BAD_FUNC_ARG: Returns if any argument is null.
    \return BUFFER_E: Returns if outLen is less than ED25519_PRV_KEY_SIZE 
    or ED25519_PUB_KEY_SIZE
    
    \param key The ed25519_key structure to export to.
    \param priv Byte array to store private key.
    \param privSz Size of priv buffer.
    \param pub Byte array to store public key.
    \param pubSz Size of pub buffer.

    _Example_
    \code
    int ret;
    ed25519_key key;
    // initialize key, make key

    char pub[32];
    word32 pubSz = sizeof(pub);
    char priv[32];
    word32 privSz = sizeof(priv);

    ret = wc_ed25519_export_key(&key, priv, &pubSz, pub, &pubSz);
    if ( ret != 0) {
    	// error exporting public key
    }
    \endcode
    
    \sa wc_ed25519_export_private
    \sa wc_ed25519_export_public
*/
WOLFSSL_API
int wc_ed25519_export_key(ed25519_key* key,
                          byte* priv, word32 *privSz,
                          byte* pub, word32 *pubSz);
/*!
    \ingroup ED25519
    
    \brief This function returns the key size of an ed25519_key structure, 
    or 32 bytes.
    
    \return Success Given a valid key, returns ED25519_KEY_SIZE (32 bytes)
    \return BAD_FUNC_ARGS Returned if the given key is NULL
    
    \param key pointer to an ed25519_key structure for which to get the 
    key size
    
    _Example_
    \code
    int keySz;
    ed25519_key key;
    // initialize key, make key
    keySz = wc_ed25519_size(&key);
    if ( keySz == 0) {
	    // error determining key size
    }
    \endcode
    
    \sa wc_ed25519_make_key
*/
WOLFSSL_API
int wc_ed25519_size(ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief Returns the private key size (secret + public) in bytes.
    
    \return BAD_FUNC_ARG Returns if key argument is null.
    \return ED25519_PRV_KEY_SIZE The size of the private key.
    
    \param key The ed25119_key struct
    
    _Example_
    \code
    ed25519_key key;
    wc_ed25519_init(&key);

    WC_RNG rng;
    wc_InitRng(&rng);

    wc_ed25519_make_key(&rng, 32, &key); // initialize 32 byte ed25519 key
    int key_size = wc_ed25519_priv_size(&key);
    \endcode
    
    \sa wc_ed25119_pub_size
*/
WOLFSSL_API
int wc_ed25519_priv_size(ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief Returns the compressed key size in bytes (public key).
    
    \return BAD_FUNC_ARG returns if key is null.
    \return ED25519_PUB_KEY_SIZE Size of key.
    
    \param key Pointer to the ed25519_key struct.
    
    _Example_
    \code
    ed25519_key key;
    wc_ed25519_init(&key);
    WC_RNG rng;
    wc_InitRng(&rng);

    wc_ed25519_make_key(&rng, 32, &key); // initialize 32 byte ed25519 key
    int key_size = wc_ed25519_pub_size(&key);
    \endcode
    
    \sa wc_ed25519_priv_size
*/
WOLFSSL_API
int wc_ed25519_pub_size(ed25519_key* key);
/*!
    \ingroup ED25519
    
    \brief This function returns the size of an ed25519 signature (64 in bytes).
    
    \return Success Given a valid key, returns ED25519_SIG_SIZE (64 in bytes)
    \return 0 Returned if the given key is NULL
    
    \param key pointer to an ed25519_key structure for which to get the 
    signature size
    
    _Example_
    \code
    int sigSz;
    ed25519_key key;
    // initialize key, make key

    sigSz = wc_ed25519_sig_size(&key);
    if ( sigSz == 0) {
    	// error determining sig size
    }
    \endcode
    
    \sa wc_ed25519_sign_msg
*/
WOLFSSL_API
int wc_ed25519_sig_size(ed25519_key* key);
