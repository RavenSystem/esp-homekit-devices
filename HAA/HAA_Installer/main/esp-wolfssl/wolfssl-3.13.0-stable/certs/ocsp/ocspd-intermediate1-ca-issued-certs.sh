#!/bin/sh

openssl ocsp -port 22221 -nmin 1                                \
    -index   certs/ocsp/index-intermediate1-ca-issued-certs.txt \
    -rsigner certs/ocsp/ocsp-responder-cert.pem                 \
    -rkey    certs/ocsp/ocsp-responder-key.pem                  \
    -CA      certs/ocsp/intermediate1-ca-cert.pem               \
    $@
