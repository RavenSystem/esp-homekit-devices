/*!
    \ingroup HMAC
    
    \brief This function initializes an Hmac object, setting its 
    encryption type, key and HMAC length.
    
    \return 0 Returned on successfully initializing the Hmac object
    \return BAD_FUNC_ARG Returned if the input type is invalid. Valid options 
    are: MD5, SHA, SHA256, SHA384, SHA512, BLAKE2B_ID
    \return MEMORY_E Returned if there is an error allocating memory for the 
    structure to use for hashing
    \return HMAC_MIN_KEYLEN_E May be returned when using a FIPS implementation 
    and the key length specified is shorter than the minimum acceptable 
    FIPS standard
    
    \param hmac pointer to the Hmac object to initialize
    \param type type specifying which encryption method the Hmac object 
    should use. Valid options are: MD5, SHA, SHA256, SHA384, SHA512, BLAKE2B_ID
    \param key pointer to a buffer containing the key with which to 
    initialize the Hmac object
    \param length length of the key
    
    _Example_
    \code
    Hmac hmac;
    byte key[] = { // initialize with key to use for encryption };
    if (wc_HmacSetKey(&hmac, MD5, key, sizeof(key)) != 0) {
    	// error initializing Hmac object
    }
    \endcode
    
    \sa wc_HmacUpdate
    \sa wc_HmacFinal
*/
WOLFSSL_API int wc_HmacSetKey(Hmac*, int type, const byte* key, word32 keySz);
/*!
    \ingroup HMAC
    
    \brief This function updates the message to authenticate using HMAC. 
    It should be called after the Hmac object has been initialized with 
    wc_HmacSetKey. This function may be called multiple times to update 
    the message to hash. After calling wc_HmacUpdate as desired, one should 
    call wc_HmacFinal to obtain the final authenticated message tag. 
    
    \return 0 Returned on successfully updating the message to authenticate
    \return MEMORY_E Returned if there is an error allocating memory for 
    use with a hashing algorithm
    
    \param hmac pointer to the Hmac object for which to update the message
    \param msg pointer to the buffer containing the message to append
    \param length length of the message to append
    
    _Example_
    \code
    Hmac hmac;
    byte msg[] = { // initialize with message to authenticate };
    byte msg2[] = { // initialize with second half of message };
    // initialize hmac
    if( wc_HmacUpdate(&hmac, msg, sizeof(msg)) != 0) {
    	// error updating message
    }
    if( wc_HmacUpdate(&hmac, msg2, sizeof(msg)) != 0) {
    	// error updating with second message
    }
    \endcode
    
    \sa wc_HmacSetKey
    \sa wc_HmacFinal
*/
WOLFSSL_API int wc_HmacUpdate(Hmac*, const byte*, word32);
/*!
    \ingroup HMAC
    
    \brief This function computes the final hash of an Hmac object's message.
    
    \return 0 Returned on successfully computing the final hash
    \return MEMORY_E Returned if there is an error allocating memory for 
    use with a hashing algorithm
    
    \param hmac pointer to the Hmac object for which to calculate the 
    final hash
    \param hash pointer to the buffer in which to store the final hash. 
    Should have room available as required by the hashing algorithm chosen
    
    _Example_
    \code
    Hmac hmac;
    byte hash[MD5_DIGEST_SIZE];
    // initialize hmac with MD5 as type
    // wc_HmacUpdate() with messages

    if (wc_HmacFinal(&hmac, hash) != 0) {
    	// error computing hash
    }
    \endcode
    
    \sa wc_HmacSetKey
    \sa wc_HmacUpdate
*/
WOLFSSL_API int wc_HmacFinal(Hmac*, byte*);
/*!
    \ingroup HMAC
    
    \brief This function returns the largest HMAC digest size available 
    based on the configured cipher suites.
    
    \return Success Returns the largest HMAC digest size available based 
    on the configured cipher suites
    
    \param none No parameters.
    
    _Example_
    \code
    int maxDigestSz = wolfSSL_GetHmacMaxSize();
    \endcode
    
    \sa none
*/
WOLFSSL_API int wolfSSL_GetHmacMaxSize(void);
/*!
    \ingroup HMAC
    
    \brief This function provides access to a HMAC Key Derivation Function 
    (HKDF). It utilizes HMAC to convert inKey, with an optional salt and 
    optional info into a derived key, which it stores in out. The hash type 
    defaults to MD5 if 0 or NULL is given.
    
    \return 0 Returned upon successfully generating a key with the given inputs
    \return BAD_FUNC_ARG Returned if an invalid hash type is given as 
    argument. Valid types are: MD5, SHA, SHA256, SHA384, SHA512, BLAKE2B_ID
    \return MEMORY_E Returned if there is an error allocating memory
    \return HMAC_MIN_KEYLEN_E May be returned when using a FIPS implementation 
    and the key length specified is shorter than the minimum acceptable FIPS 
    standard
    
    \param type hash type to use for the HKDF.  Valid types are: MD5, SHA, 
    SHA256, SHA384, SHA512, BLAKE2B_ID
    \param inKey pointer to the buffer containing the key to use for KDF
    \param inKeySz length of the input key
    \param salt pointer to a buffer containing an optional salt. Use NULL 
    instead if not using a salt
    \param saltSz length of the salt. Use 0 if not using a salt
    \param info pointer to a buffer containing optional additional info. 
    Use NULL if not appending extra info
    \param infoSz length of additional info. Use 0 if not using additional info
    \param out pointer to the buffer in which to store the derived key
    \param outSz space available in the output buffer to store the 
    generated key
    
    _Example_
    \code
    byte key[] = { // initialize with key };
    byte salt[] = { // initialize with salt };
    byte derivedKey[MAX_DIGEST_SIZE];

    int ret = wc_HKDF(SHA512, key, sizeof(key), salt, sizeof(salt),
    NULL, 0, derivedKey, sizeof(derivedKey));
    if ( ret != 0 ) {
	    // error generating derived key
    }
    \endcode
    
    \sa wc_HmacSetKey
*/
WOLFSSL_API int wc_HKDF(int type, const byte* inKey, word32 inKeySz,
                    const byte* salt, word32 saltSz,
                    const byte* info, word32 infoSz,
                    byte* out, word32 outSz);
