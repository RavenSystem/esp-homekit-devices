#openssl sha384 -binary -out firmware/main.bin.sig firmware/main.bin
#printf "%08x" `cat firmware/main.bin | wc -c` | xxd -r -p >> firmware/main.bin.sig

../../../ecc_signer/ecc_signer --normal firmware/*.bin ../../../ecc_signer/certs/priv_key.der ../../../ecc_signer/certs/pub_key.der
