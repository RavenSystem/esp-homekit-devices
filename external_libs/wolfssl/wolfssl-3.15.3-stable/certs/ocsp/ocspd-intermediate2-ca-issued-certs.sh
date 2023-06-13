#!/bin/sh

openssl ocsp -port 22222 -nmin 1                                \
    -index   certs/ocsp/index-intermediate2-ca-issued-certs.txt \
    -rsigner certs/ocsp/ocsp-responder-cert.pem                 \
    -rkey    certs/ocsp/ocsp-responder-key.pem                  \
    -CA      certs/ocsp/intermediate2-ca-cert.pem               \
    $@
