#!/bin/bash

EXAMPLE=$1
echo "This uses ed25519 certificate generator from wolfssl-examples github"
echo "The script takes in the directory to wolfssl-examples"

pushd ${EXAMPLE}
make
if [ $? -ne 0 ]; then
    echo "Unable to build example"
    exit 1
fi

./tls.sh
popd
mv ${EXAMPLE}/*.pem .
mv ${EXAMPLE}/*.der .

