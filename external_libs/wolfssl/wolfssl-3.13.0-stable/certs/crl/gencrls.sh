#!/bin/bash

# gencrls, crl config already done, see taoCerts.txt for setup

function setup_files() {
    #set up the file system for updating the crls
    echo "setting up the file system for generating the crls..."
    echo ""
    touch ./index.txt
    touch ./crlnumber
    echo "01" >> crlnumber
    touch ./blank.index.txt
    mkdir demoCA
    touch ./demoCA/index.txt
}

function cleanup_files() {
    rm blank.index.txt
    rm index.*
    rm crlnumber*
    rm -r demoCA
    echo "Removed ../wolfssl.cnf, blank.index.txt, index.*, crlnumber*, demoCA/"
    echo ""
    exit 0
}
trap cleanup_files EXIT

#setup the files
setup_files

# caCrl
# revoke server-revoked-cert.pem
openssl ca -config ../renewcerts/wolfssl.cnf -revoke ../server-revoked-cert.pem -keyfile ../ca-key.pem -cert ../ca-cert.pem

openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out crl.pem -keyfile ../ca-key.pem -cert ../ca-cert.pem

# metadata
openssl crl -in crl.pem -text > tmp
mv tmp crl.pem
# install (only needed if working outside wolfssl)
#cp crl.pem ~/wolfssl/certs/crl/crl.pem

# caCrl server revoked
openssl ca -config ../renewcerts/wolfssl.cnf -revoke ../server-cert.pem -keyfile ../ca-key.pem -cert ../ca-cert.pem

# caCrl server revoked generation
openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out crl.revoked -keyfile ../ca-key.pem -cert ../ca-cert.pem

# metadata
openssl crl -in crl.revoked -text > tmp
mv tmp crl.revoked
# install (only needed if working outside wolfssl)
#cp crl.revoked ~/wolfssl/certs/crl/crl.revoked

# remove revoked so next time through the normal CA won't have server revoked
cp blank.index.txt demoCA/index.txt

# caEccCrl
openssl ca -config ../renewcerts/wolfssl.cnf -revoke ../server-revoked-cert.pem -keyfile ../ca-ecc-key.pem -cert ../ca-ecc-cert.pem

openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out caEccCrl.pem -keyfile ../ca-ecc-key.pem -cert ../ca-ecc-cert.pem

# metadata
openssl crl -in caEccCrl.pem -text > tmp
mv tmp caEccCrl.pem
# install (only needed if working outside wolfssl)
#cp caEccCrl.pem ~/wolfssl/certs/crl/caEccCrl.pem

# caEcc384Crl
openssl ca -config ../renewcerts/wolfssl.cnf -revoke ../server-revoked-cert.pem -keyfile ../ca-ecc384-key.pem -cert ../ca-ecc384-cert.pem

openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out caEcc384Crl.pem -keyfile ../ca-ecc384-key.pem -cert ../ca-ecc384-cert.pem

# metadata
openssl crl -in caEcc384Crl.pem -text > tmp
mv tmp caEcc384Crl.pem
# install (only needed if working outside wolfssl)
#cp caEcc384Crl.pem ~/wolfssl/certs/crl/caEcc384Crl.pem

# cliCrl
openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out cliCrl.pem -keyfile ../client-key.pem -cert ../client-cert.pem

# metadata
openssl crl -in cliCrl.pem -text > tmp
mv tmp cliCrl.pem
# install (only needed if working outside wolfssl)
#cp cliCrl.pem ~/wolfssl/certs/crl/cliCrl.pem

# eccCliCRL
openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out eccCliCRL.pem -keyfile ../ecc-client-key.pem -cert ../client-ecc-cert.pem

# metadata
openssl crl -in eccCliCRL.pem -text > tmp
mv tmp eccCliCRL.pem
# install (only needed if working outside wolfssl)
#cp eccCliCRL.pem ~/wolfssl/certs/crl/eccCliCRL.pem

# eccSrvCRL
openssl ca -config ../renewcerts/wolfssl.cnf -gencrl -crldays 1000 -out eccSrvCRL.pem -keyfile ../ecc-key.pem -cert ../server-ecc.pem

# metadata
openssl crl -in eccSrvCRL.pem -text > tmp
mv tmp eccSrvCRL.pem
# install (only needed if working outside wolfssl)
#cp eccSrvCRL.pem ~/wolfssl/certs/crl/eccSrvCRL.pem

exit 0
