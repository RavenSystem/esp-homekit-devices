#!/bin/bash
#
# assemble-chains.sh
# Create certs and assemble all the certificate CA path test cert chains.


###########################################################
########## update server-0-ca.pem          ################
###########################################################
echo "Updating server-0-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 0 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-0-ca-req.pem

openssl x509 -req -in server-0-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_0 -days 1000 -CA ../ca-cert.pem -CAkey ../ca-key.pem -set_serial 100 -sha1 > server-0-ca.pem

rm server-0-ca-req.pem
openssl x509 -in server-0-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-0-ca.pem


###########################################################
########## update server-0-cert.pem        ################
###########################################################
echo "Updating server-0-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 0\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-0-cert-req.pem

openssl x509 -req -in server-0-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-0-ca.pem -CAkey ../server-key.pem -set_serial 101 -sha1 > server-0-cert.pem

rm server-0-cert-req.pem
openssl x509 -in server-0-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-0-cert.pem


###########################################################
########## update server-1-ca.pem          ################
###########################################################
echo "Updating server-1-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 1 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-1-ca-req.pem

openssl x509 -req -in server-1-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_1 -days 1000 -CA ../ca-cert.pem -CAkey ../ca-key.pem -set_serial 102 -sha1 > server-1-ca.pem

rm server-1-ca-req.pem
openssl x509 -in server-1-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-1-ca.pem


###########################################################
########## update server-1-cert.pem        ################
###########################################################
echo "Updating server-1-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 1\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-1-cert-req.pem

openssl x509 -req -in server-1-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-1-ca.pem -CAkey ../server-key.pem -set_serial 105 -sha1 > server-1-cert.pem

rm server-1-cert-req.pem
openssl x509 -in server-1-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-1-cert.pem


###########################################################
########## update server-0-1-ca.pem        ################
###########################################################
echo "Updating server-0-1-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 0-1 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-0-1-ca-req.pem

openssl x509 -req -in server-0-1-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_1 -days 1000 -CA server-0-ca.pem -CAkey ../server-key.pem -set_serial 110 -sha1 > server-0-1-ca.pem

rm server-0-1-ca-req.pem
openssl x509 -in server-0-1-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-0-1-ca.pem


###########################################################
########## update server-0-1-cert.pem      ################
###########################################################
echo "Updating server-0-1-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 0-1\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-0-1-cert-req.pem

openssl x509 -req -in server-0-1-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-0-1-ca.pem -CAkey ../server-key.pem -set_serial 111 -sha1 > server-0-1-cert.pem

rm server-0-1-cert-req.pem
openssl x509 -in server-0-1-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-0-1-cert.pem


###########################################################
########## update server-1-0-ca.pem        ################
###########################################################
echo "Updating server-1-0-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 1-0 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-1-0-ca-req.pem

openssl x509 -req -in server-1-0-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_0 -days 1000 -CA server-1-ca.pem -CAkey ../server-key.pem -set_serial 103 -sha1 > server-1-0-ca.pem

rm server-1-0-ca-req.pem
openssl x509 -in server-1-0-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-1-0-ca.pem


###########################################################
########## update server-1-0-cert.pem      ################
###########################################################
echo "Updating server-1-0-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 1-0\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-1-0-cert-req.pem

openssl x509 -req -in server-1-0-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-1-0-ca.pem -CAkey ../server-key.pem -set_serial 104 -sha1 > server-1-0-cert.pem

rm server-1-0-cert-req.pem
openssl x509 -in server-1-0-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-1-0-cert.pem


###########################################################
########## update server-127-ca.pem        ################
###########################################################
echo "Updating server-127-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 127 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-127-ca-req.pem

openssl x509 -req -in server-127-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_127 -days 1000 -CA ../ca-cert.pem -CAkey ../ca-key.pem -set_serial 106 -sha1 > server-127-ca.pem

rm server-127-ca-req.pem
openssl x509 -in server-127-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-127-ca.pem


###########################################################
########## update server-127-cert.pem      ################
###########################################################
echo "Updating server-127-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 127\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-127-cert-req.pem

openssl x509 -req -in server-127-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-127-ca.pem -CAkey ../server-key.pem -set_serial 107 -sha1 > server-127-cert.pem

rm server-127-cert-req.pem
openssl x509 -in server-127-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-127-cert.pem


###########################################################
########## update server-128-ca.pem        ################
###########################################################
echo "Updating server-128-ca.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 128 CA\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-128-ca-req.pem

openssl x509 -req -in server-128-ca-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions pathlen_128 -days 1000 -CA ../ca-cert.pem -CAkey ../ca-key.pem -set_serial 106 -sha1 > server-128-ca.pem

rm server-128-ca-req.pem
openssl x509 -in server-128-ca.pem -text > ca_tmp.pem
mv ca_tmp.pem server-128-ca.pem


###########################################################
########## update server-128-cert.pem      ################
###########################################################
echo "Updating server-128-cert.pem"
echo ""
#pipe the following arguments to openssl req...
echo -e "US\nWashington\nSeattle\nwolfSSL Inc.\nEngineering\nServer 128\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ../server-key.pem -nodes -sha1 > server-128-cert-req.pem

openssl x509 -req -in server-128-cert-req.pem -extfile ../renewcerts/wolfssl.cnf -extensions test_pathlen -days 1000 -CA server-128-ca.pem -CAkey ../server-key.pem -set_serial 107 -sha1 > server-128-cert.pem

rm server-128-cert-req.pem
openssl x509 -in server-128-cert.pem -text > cert_tmp.pem
mv cert_tmp.pem server-128-cert.pem


###########################################################
########## Assemble Chains                 ################
###########################################################
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
