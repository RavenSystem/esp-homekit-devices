#openssl sha384 -binary -out firmware/haa_lcm.bin.sig firmware/haa_lcm.bin; printf "%08x" `cat firmware/haa_lcm.bin | wc -c` | xxd -r -p >> firmware/haa_lcm.bin.sig

../../../ecc_signer/ecc_signer --normal firmware/*.bin ../../../ecc_signer/certs/priv_key.der ../../../ecc_signer/certs/pub_key.der
