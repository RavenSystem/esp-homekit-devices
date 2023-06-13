/*!
    \ingroup HC128
    
    \brief This function encrypts or decrypts a message of any size from the 
    input buffer input, and stores the resulting plaintext/ciphertext in 
    the output buffer output.
    
    \return 0 Returned upon successfully encrypting/decrypting the given input
    \return MEMORY_E Returned if the input and output buffers are not aligned 
    along a 4-byte boundary, and there is an error allocating memory
    \return BAD_ALIGN_E Returned if the input or output buffers are not 
    aligned along a 4-byte boundary, and NO_WOLFSSL_ALLOC_ALIGN is defined
    
    \param ctx pointer to a HC-128 context object with an initialized key 
    to use for encryption or decryption
    \param output buffer in which to store the processed input
    \param input  buffer containing the plaintext to encrypt or the 
    ciphertext to decrypt
    \param msglen length of the plaintext to encrypt or the ciphertext 
    to decrypt
    
    _Example_
    \code
    HC128 enc;
    byte key[] = { // initialize with key };
    byte iv[]  = { // initialize with iv };
    wc_Hc128_SetKey(&enc, key, iv);

    byte msg[] = { // initialize with message };
    byte cipher[sizeof(msg)];

    if (wc_Hc128_Process(*enc, cipher, plain, sizeof(plain)) != 0) {
    	// error encrypting msg
    }
    \endcode
    
    \sa wc_Hc128_SetKey
*/
WOLFSSL_API int wc_Hc128_Process(HC128*, byte*, const byte*, word32);
/*!
    \ingroup HC128
    
    \brief This function initializes an HC128 context object by 
    setting its key and iv.
    
    \return 0 Returned upon successfully setting the key and iv 
    for the HC128 context object
    
    \param ctx pointer to an HC-128 context object to initialize
    \param key pointer to the buffer containing the 16 byte key to 
    use with encryption/decryption
    \param iv pointer to the buffer containing the 16 byte iv (nonce) 
    with which to initialize the HC128 object
    
    _Example_
    \code
    HC128 enc;
    byte key[] = { // initialize with key };
    byte iv[]  = { // initialize with iv };
    wc_Hc128_SetKey(&enc, key, iv);
    \endcode
    
    \sa wc_Hc128_Process
*/
WOLFSSL_API int wc_Hc128_SetKey(HC128*, const byte* key, const byte* iv);
