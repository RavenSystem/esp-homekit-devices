/*!
    \ingroup Rabbit
    
    \brief This function encrypts or decrypts a message of any size, storing 
    the result in output. It requires that the Rabbit ctx structure be 
    initialized with a key and an iv before encryption.

    \return 0 Returned on successfully encrypting/decrypting input
    \return BAD_ALIGN_E Returned if the input message is not 4-byte aligned 
    but is required to be by XSTREAM_ALIGN, but NO_WOLFSSL_ALLOC_ALIGN is 
    defined
    \return MEMORY_E Returned if there is an error allocating memory to 
    align the message, if NO_WOLFSSL_ALLOC_ALIGN is not defined
    
    \param ctx pointer to the Rabbit structure to use for encryption/decryption
    \param output pointer to the buffer in which to store the processed 
    message. Should be at least msglen long
    \param input pointer to the buffer containing the message to process
    \param msglen the length of the message to process
    
    _Example_
    \code
    int ret;
    Rabbit enc;
    byte key[] = { }; // initialize with 16 byte key
    byte iv[]  = { }; // initialize with 8 byte iv

    wc_RabbitSetKey(&enc, key, iv);

    byte message[] = { }; // initialize with plaintext message
    byte ciphertext[sizeof(message)];

    wc_RabbitProcess(enc, ciphertext, message, sizeof(message));
    \endcode
    
    \sa wc_RabbitSetKey
*/
WOLFSSL_API int wc_RabbitProcess(Rabbit*, byte*, const byte*, word32);
/*!
    \ingroup Rabbit
    
    \brief This function initializes a Rabbit context for use with 
    encryption or decryption by setting its iv and key.
    
    \return 0 Returned on successfully setting the key and iv
    
    \param ctx pointer to the Rabbit structure to initialize
    \param key pointer to the buffer containing the 16 byte key to 
    use for encryption/decryption
    \param iv pointer to the buffer containing the 8 byte iv with 
    which to initialize the Rabbit structure
    
    _Example_
    \code
    int ret;
    Rabbit enc;
    byte key[] = { }; // initialize with 16 byte key
    byte iv[]  = { }; // initialize with 8 byte iv

    wc_RabbitSetKey(&enc, key, iv)
    \endcode
    
    \sa wc_RabbitProcess
*/
WOLFSSL_API int wc_RabbitSetKey(Rabbit*, const byte* key, const byte* iv);
