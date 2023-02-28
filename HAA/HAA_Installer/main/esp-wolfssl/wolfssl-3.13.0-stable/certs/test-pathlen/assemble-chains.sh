#!/bin/bash
#
# assemble-chains.sh
# Assemble all the certificate CA path test cert chains.

# Success: PathLen of 0
## server-0-ca.pem: signed by ca-cert.pem
## server-0-cert.pem: signed by server-0-ca.pem 
cat server-0-cert.pem server-0-ca.pem > server-0-chain.pem

# Success: PathLen of 1
## server-1-ca.pem: signed by ca-cert.pem
## server-1-0-ca.pem: signed by server-1-ca.pem
## server-1-0-cert.pem: signed by server-1-0-ca.pem
cat server-1-0-cert.pem server-1-0-ca.pem server-1-ca.pem > server-1-0-chain.pem
## server-1-cert.pem: signed by server-1-ca.pem
cat server-1-cert.pem server-1-ca.pem > server-1-chain.pem

# Success: PathLen of 127
## server-127-ca.pem: signed by ca-cert.pem
## server-127-cert.pem: signed by server-127-cert.pem
cat server-127-cert.pem server-127-ca.pem > server-127-chain.pem

# Failure: PathLen of 128
## server-128-ca.pem: signed by ca-cert.pem
## server-128-cert.pem: signed by server-128-ca.pem
cat server-128-cert.pem server-128-ca.pem > server-128-chain.pem

# Failure: PathLen of 0, signing PathLen of 1
## server-0-1-ca.pem: signed by server-0-ca.pem
## server-0-1-cert.pem: signed by server-0-1-ca.pem
cat server-0-1-cert.pem server-0-1-ca.pem server-0-ca.pem > server-0-1-chain.pem
