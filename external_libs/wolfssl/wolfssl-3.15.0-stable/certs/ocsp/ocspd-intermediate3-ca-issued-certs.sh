#!/bin/sh

openssl ocsp -port 22223 -nmin 1                                \
    -index   certs/ocsp/index-intermediate3-ca-issued-certs.txt \
    -rsigner certs/ocsp/ocsp-responder-cert.pem                 \
    -rkey    certs/ocsp/ocsp-responder-key.pem                  \
    -CA      certs/ocsp/intermediate3-ca-cert.pem               \
    $@
