#!/bin/sh

openssl ocsp -port 22221 -nmin 1                                \
    -index   certs/ocsp/index-intermediate1-ca-issued-certs.txt \
    -rsigner certs/ocsp/intermediate1-ca-cert.pem               \
    -rkey    certs/ocsp/intermediate1-ca-key.pem                \
    -CA      certs/ocsp/intermediate1-ca-cert.pem               \
    $@
