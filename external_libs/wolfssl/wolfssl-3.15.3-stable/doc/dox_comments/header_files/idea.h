/*!
    \ingroup IDEA
    
    \brief Generate the 52, 16-bit key sub-blocks from the 128 key.
    
    \return 0 Success
    \return BAD_FUNC_ARG Returns if idea or key is null, keySz is not equal to 
    IDEA_KEY_SIZE, or dir is not IDEA_ENCRYPTION or IDEA_DECRYPTION.
    
    \param idea Pointer to Idea structure.
    \param key Pointer to key in memory.
    \param keySz Size of key.
    \param iv Value for IV in Idea structure.  Can be null.
    \param dir Direction, either IDEA_ENCRYPTION or IDEA_DECRYPTION

    _Example_
    \code
    byte v_key[IDEA_KEY_SIZE] = { }; // Some Key
    Idea idea;
    int ret = wc_IdeaSetKey(&idea v_key, IDEA_KEY_SIZE, NULL, IDEA_ENCRYPTION);
    if (ret != 0)
    {
        // There was an error
    }
    \endcode
    
    \sa wc_IdeaSetIV
*/
WOLFSSL_API int wc_IdeaSetKey(Idea *idea, const byte* key, word16 keySz,
                              const byte *iv, int dir);
/*!
    \ingroup IDEA
    
    \brief Sets the IV in an Idea key structure.

    \return 0 Success
    \return BAD_FUNC_ARG Returns if idea is null.

    \param idea Pointer to idea key structure.
    \param iv The IV value to set, can be null.
    
    _Example_
    \code
    Idea idea;
    // Initialize idea

    byte iv[] = { }; // Some IV
    int ret = wc_IdeaSetIV(&idea, iv);
    if(ret != 0)
    {
        // Some error occured
    }
    \endcode
    
    \sa wc_IdeaSetKey
*/
WOLFSSL_API int wc_IdeaSetIV(Idea *idea, const byte* iv);
/*!
    \ingroup IDEA
    
    \brief Encryption or decryption for a block (64 bits).
    
    \return 0 upon success.
    \return <0 an error occured
    
    \param idea Pointer to idea key structure.
    \param out Pointer to destination.
    \param in Pointer to input data to encrypt or decrypt.

    _Example_
    \code
    byte v_key[IDEA_KEY_SIZE] = { }; // Some Key
    byte data[IDEA_BLOCK_SIZE] = { }; // Some encrypted data
    Idea idea;
    wc_IdeaSetKey(&idea, v_key, IDEA_KEY_SIZE, NULL, IDEA_DECRYPTION);
    int ret = wc_IdeaCipher(&idea, data, data);

    if (ret != 0)
    {
        // There was an error
    }
    \endcode
    
    \sa wc_IdeaSetKey
    \sa wc_IdeaSetIV
    \sa wc_IdeaCbcEncrypt
    \sa wc_IdeaCbcDecrypt
*/
WOLFSSL_API int wc_IdeaCipher(Idea *idea, byte* out, const byte* in);
/*!
    \ingroup IDEA
    
    \brief Encrypt data using IDEA CBC mode.
    
    \return 0 Success
    \return BAD_FUNC_ARG Returns if any arguments are null.

    \param idea Pointer to Idea key structure.
    \param out Pointer to destination for encryption.
    \param in Pointer to input for encryption.
    \param len length of input.
    
    _Example_
    \code
    Idea idea;
    // Initialize idea structure for encryption
    const char *message = "International Data Encryption Algorithm";
    byte msg_enc[40], msg_dec[40];

    memset(msg_enc, 0, sizeof(msg_enc));
    ret = wc_IdeaCbcEncrypt(&idea, msg_enc, (byte *)message,
                                (word32)strlen(message)+1);
    if(ret != 0)
    {
        // Some error occured
    }
    \endcode
    
    \sa wc_IdeaCbcDecrypt
    \sa wc_IdeaCipher
    \sa wc_IdeaSetKey
*/
WOLFSSL_API int wc_IdeaCbcEncrypt(Idea *idea, byte* out,
                                  const byte* in, word32 len);
/*!
    \ingroup IDEA
    
    \brief Decrypt data using IDEA CBC mode.

    \return 0 Success
    \return BAD_FUNC_ARG Returns if any arguments are null.
    
    \param idea Pointer to Idea key structure.
    \param out Pointer to destination for encryption.
    \param in Pointer to input for encryption.
    \param len length of input.
    
    _Example_
    \code
    Idea idea;
    // Initialize idea structure for decryption
    const char *message = "International Data Encryption Algorithm";
    byte msg_enc[40], msg_dec[40];

    memset(msg_dec, 0, sizeof(msg_dec));
    ret = wc_IdeaCbcDecrypt(&idea, msg_dec, msg_enc,
                                (word32)strlen(message)+1);
    if(ret != 0)
    {
        // Some error occured
    }
    \endcode
    
    \sa wc_IdeaCbcEncrypt
    \sa wc_IdeaCipher
    \sa wc_IdeaSetKey
*/
WOLFSSL_API int wc_IdeaCbcDecrypt(Idea *idea, byte* out,
                                  const byte* in, word32 len);
