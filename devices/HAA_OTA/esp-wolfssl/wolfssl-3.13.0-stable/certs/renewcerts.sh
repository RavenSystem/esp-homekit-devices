#!/bin/bash
# renewcerts.sh
#
# renews the following certs:
#                       client-cert.pem
#                       client-cert.der
#                       client-ecc-cert.pem
#                       client-ecc-cert.der
#                       ca-cert.pem
#                       ca-cert.der
#                       server-cert.pem
#                       server-cert.der
#                       server-ecc-rsa.pem
#                       server-ecc.pem
#                       1024/client-cert.der
#                       1024/client-cert.pem
#                       server-ecc-comp.pem
#                       client-ca.pem
#                       test/digsigku.pem
# updates the following crls:
#                       crl/cliCrl.pem
#                       crl/crl.pem
#                       crl/crl.revoked
#                       crl/eccCliCRL.pem
#                       crl/eccSrvCRL.pem
# if HAVE_NTRU
#                       ntru-cert.pem
#                       ntru-key.raw
###############################################################################
######################## FUNCTIONS SECTION ####################################
###############################################################################

#the function that will be called when we are ready to renew the certs.
function run_renewcerts(){
    cd certs/
    echo ""
    #move the custom cnf into our working directory
    cp renewcerts/wolfssl.cnf wolfssl.cnf

    # To generate these all in sha1 add the flag "-sha1" on appropriate lines
    # That is all lines beginning with:  "openssl req"

    ############################################################
    #### update the self-signed (2048-bit) client-cert.pem #####
    ############################################################
    echo "Updating 2048-bit client-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL_2048\nProgramming-2048\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key client-key.pem -nodes -out client-cert.csr


    openssl x509 -req -in client-cert.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey client-key.pem -out client-cert.pem
    rm client-cert.csr

    openssl x509 -in client-cert.pem -text > tmp.pem
    mv tmp.pem client-cert.pem


    ############################################################
    #### update the self-signed (3072-bit) client-cert.pem #####
    ############################################################
    echo "Updating 3072-bit client-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL_3072\nProgramming-3072\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -newkey rsa:3072 -keyout client-key-3072.pem -nodes -out client-cert-3072.csr


    openssl x509 -req -in client-cert-3072.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey client-key-3072.pem -out client-cert-3072.pem
    rm client-cert-3072.csr

    openssl x509 -in client-cert-3072.pem -text > tmp.pem
    mv tmp.pem client-cert-3072.pem


    ############################################################
    #### update the self-signed (1024-bit) client-cert.pem #####
    ############################################################
    echo "Updating 1024-bit client-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL_1024\nProgramming-1024\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key \1024/client-key.pem -nodes -out \1024/client-cert.csr


    openssl x509 -req -in \1024/client-cert.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey \1024/client-key.pem -out \1024/client-cert.pem
    rm \1024/client-cert.csr

    openssl x509 -in \1024/client-cert.pem -text > \1024/tmp.pem
    mv \1024/tmp.pem \1024/client-cert.pem
    ############################################################
    ########## update the self-signed ca-cert.pem ##############
    ############################################################
    echo "Updating ca-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e  "US\nMontana\nBozeman\nSawtooth\nConsulting\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ca-key.pem -nodes -out ca-cert.csr

    openssl x509 -req -in ca-cert.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey ca-key.pem -out ca-cert.pem
    rm ca-cert.csr

    openssl x509 -in ca-cert.pem -text > tmp.pem
    mv tmp.pem ca-cert.pem
    ############################################################
    ##### update the self-signed (1024-bit) ca-cert.pem ########
    ############################################################
    echo "Updating 1024-bit ca-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e  "US\nMontana\nBozeman\nSawtooth\nConsulting_1024\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key \1024/ca-key.pem -nodes -out \1024/ca-cert.csr

    openssl x509 -req -in \1024/ca-cert.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey \1024/ca-key.pem -out \1024/ca-cert.pem
    rm \1024/ca-cert.csr

    openssl x509 -in \1024/ca-cert.pem -text > \1024/tmp.pem
    mv \1024/tmp.pem \1024/ca-cert.pem
    ###########################################################
    ########## update and sign server-cert.pem ################
    ###########################################################
    echo "Updating server-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL\nSupport\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key server-key.pem -nodes > server-req.pem

    openssl x509 -req -in server-req.pem -extfile wolfssl.cnf -extensions wolfssl_opts -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > server-cert.pem

    rm server-req.pem

    openssl x509 -in ca-cert.pem -text > ca_tmp.pem
    openssl x509 -in server-cert.pem -text > srv_tmp.pem
    mv srv_tmp.pem server-cert.pem
    cat ca_tmp.pem >> server-cert.pem
    rm ca_tmp.pem
    ###########################################################
    ########## update and sign server-revoked-key.pem #########
    ###########################################################
    echo "Updating server-revoked-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL_revoked\nSupport_revoked\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key server-revoked-key.pem -nodes > server-revoked-req.pem

    openssl x509 -req -in server-revoked-req.pem -extfile wolfssl.cnf -extensions wolfssl_opts -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 02 > server-revoked-cert.pem

    rm server-revoked-req.pem

    openssl x509 -in ca-cert.pem -text > ca_tmp.pem
    openssl x509 -in server-revoked-cert.pem -text > srv_tmp.pem
    mv srv_tmp.pem server-revoked-cert.pem
    cat ca_tmp.pem >> server-revoked-cert.pem
    rm ca_tmp.pem
    ###########################################################
    ########## update and sign server-duplicate-policy.pem ####
    ###########################################################
    echo "Updating server-duplicate-policy.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL\ntesting duplicate policy\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key server-key.pem -nodes > ./test/server-duplicate-policy-req.pem

    openssl x509 -req -in ./test/server-duplicate-policy-req.pem -extfile wolfssl.cnf -extensions policy_test -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 02 > ./test/server-duplicate-policy.pem

    rm ./test/server-duplicate-policy-req.pem

    openssl x509 -in ca-cert.pem -text > ca_tmp.pem
    openssl x509 -in ./test/server-duplicate-policy.pem -text > srv_tmp.pem
    mv srv_tmp.pem ./test/server-duplicate-policy.pem
    cat ca_tmp.pem >> ./test/server-duplicate-policy.pem
    rm ca_tmp.pem
    ###########################################################
    #### update and sign (1024-bit) server-cert.pem ###########
    ###########################################################
    echo "Updating 1024-bit server-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nwolfSSL\nSupport_1024\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key \1024/server-key.pem -nodes > \1024/server-req.pem

    openssl x509 -req -in \1024/server-req.pem -extfile wolfssl.cnf -extensions wolfssl_opts -days 1000 -CA \1024/ca-cert.pem -CAkey \1024/ca-key.pem -set_serial 01 > \1024/server-cert.pem

    rm \1024/server-req.pem

    openssl x509 -in \1024/ca-cert.pem -text > \1024/ca_tmp.pem
    openssl x509 -in \1024/server-cert.pem -text > \1024/srv_tmp.pem
    mv \1024/srv_tmp.pem \1024/server-cert.pem
    cat \1024/ca_tmp.pem >> \1024/server-cert.pem
    rm \1024/ca_tmp.pem
    ############################################################
    ########## update and sign the server-ecc-rsa.pem ##########
    ############################################################
    echo "Updating server-ecc-rsa.pem"
    echo ""
    echo -e "US\nMontana\nBozeman\nElliptic - RSAsig\nECC-RSAsig\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ecc-key.pem -nodes > server-ecc-req.pem

    openssl x509 -req -in server-ecc-req.pem -extfile wolfssl.cnf -extensions wolfssl_opts -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > server-ecc-rsa.pem

    rm server-ecc-req.pem

    openssl x509 -in server-ecc-rsa.pem -text > tmp.pem
    mv tmp.pem server-ecc-rsa.pem
    ############################################################
    ####### update the self-signed client-ecc-cert.pem #########
    ############################################################
    echo "Updating client-ecc-cert.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nOregon\nSalem\nClient ECC\nFast\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ecc-client-key.pem -nodes -out client-ecc-cert.csr


    openssl x509 -req -in client-ecc-cert.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey ecc-client-key.pem -out client-ecc-cert.pem
    rm client-ecc-cert.csr

    openssl x509 -in client-ecc-cert.pem -text > tmp.pem
    mv tmp.pem client-ecc-cert.pem

    ############################################################
    ########## update the self-signed server-ecc.pem ###########
    ############################################################
    echo "Updating server-ecc.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nWashington\nSeattle\nEliptic\nECC\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ecc-key.pem -nodes -out server-ecc.csr


    openssl x509 -req -in server-ecc.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey ecc-key.pem -out server-ecc.pem
    rm server-ecc.csr

    openssl x509 -in server-ecc.pem -text > tmp.pem
    mv tmp.pem server-ecc.pem
    ############################################################
    ###### update the self-signed server-ecc-comp.pem ##########
    ############################################################
    echo "Updating server-ecc-comp.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nMontana\nBozeman\nElliptic - comp\nServer ECC-comp\nwww.wolfssl.com\ninfo@wolfssl.com\n.\n.\n" | openssl req -new -key ecc-key-comp.pem -nodes -out server-ecc-comp.csr


    openssl x509 -req -in server-ecc-comp.csr -days 1000 -extfile wolfssl.cnf -extensions wolfssl_opts -signkey ecc-key-comp.pem -out server-ecc-comp.pem
    rm server-ecc-comp.csr

    openssl x509 -in server-ecc-comp.pem -text > tmp.pem
    mv tmp.pem server-ecc-comp.pem

    ############################################################
    ############## create the client-ca.pem file ###############
    ############################################################
    echo "Updating client-ca.pem"
    echo ""
    cat client-cert.pem client-ecc-cert.pem > client-ca.pem

    ############################################################
    ###### update the self-signed test/digsigku.pem   ##########
    ############################################################
    echo "Updating test/digsigku.pem"
    echo ""
    #pipe the following arguments to openssl req...
    echo -e "US\nWashington\nSeattle\nFoofarah\nArglebargle\nfoobarbaz\ninfo@worlss.com\n.\n.\n" | openssl req -new -key ecc-key.pem -nodes -sha1 -out digsigku.csr


    openssl x509 -req -in digsigku.csr -days 1000 -extfile wolfssl.cnf -extensions digsigku -signkey ecc-key.pem -sha1 -set_serial 16393466893990650224 -out digsigku.pem
    rm digsigku.csr

    openssl x509 -in digsigku.pem -text > tmp.pem
    mv tmp.pem digsigku.pem
    mv digsigku.pem test/digsigku.pem

    ############################################################
    ########## make .der files from .pem files #################
    ############################################################
    openssl x509 -inform PEM -in \1024/client-cert.pem -outform DER -out \1024/client-cert.der
    echo "Creating der formatted certs..."
    echo ""
    openssl x509 -inform PEM -in ca-cert.pem -outform DER -out ca-cert.der
    openssl x509 -inform PEM -in client-cert.pem -outform DER -out client-cert.der
    openssl x509 -inform PEM -in server-cert.pem -outform DER -out server-cert.der
    openssl x509 -inform PEM -in client-ecc-cert.pem -outform DER -out client-ecc-cert.der
    openssl x509 -inform PEM -in server-ecc-rsa.pem -outform DER -out server-ecc-rsa.der
    openssl x509 -inform PEM -in server-ecc.pem -outform DER -out server-ecc.der
    openssl x509 -inform PEM -in server-ecc-comp.pem -outform DER -out server-ecc-comp.der

    echo "Changing directory to wolfssl root..."
    echo ""
    cd ../
    echo "Execute ./gencertbuf.pl..."
    echo ""
    ./gencertbuf.pl
    ############################################################
    ########## generate the new crls ###########################
    ############################################################

    echo "Change directory to wolfssl/certs"
    echo ""
    cd certs
    echo "We are back in the certs directory"
    echo ""

    echo "Updating the crls..."
    echo ""
    cd crl
    echo "changed directory: cd/crl"
    echo ""
    ./gencrls.sh
    echo "ran ./gencrls.sh"
    echo ""

    #cleanup the file system now that we're done
    echo "Performing final steps, cleaning up the file system..."
    echo ""

    rm ../wolfssl.cnf

}

#function for restoring a previous configure state
function restore_config(){
    mv tmp.status config.status
    mv tmp.options.h wolfssl/options.h
    make clean
    make -j 8
}

#function for copy and pasting ntru updates
function move_ntru(){
    cp ntru-cert.pem certs/ntru-cert.pem
    cp ntru-key.raw certs/ntru-key.raw
    cp ntru-cert.der certs/ntru-cert.der
}

###############################################################################
##################### THE EXECUTABLE BODY #####################################
###############################################################################

#start in root.
cd ../
#if HAVE_NTRU already defined && there is no argument
if grep HAVE_NTRU "wolfssl/options.h" && [ -z "$1" ]
then

    #run the function to renew the certs
    run_renewcerts
    # run_renewcerts will end in the wolfssl/certs/crl dir, backup to root.
    cd ../../
    echo "changed directory to wolfssl root directory."
    echo ""

    ############################################################
    ########## update ntru if already installed ################
    ############################################################

    # We cannot assume that user has certgen and keygen enabled
    ./configure --with-ntru --enable-certgen --enable-keygen
    make check

    #copy/paste ntru-certs and key to certs/
    move_ntru

#else if there was an argument given, check it for validity or print out error
elif [ ! -z "$1" ]; then
    #valid argument then renew certs without ntru
    if [ "$1" == "--override-ntru" ]; then
        echo "overriding ntru, update all certs except ntru."
        run_renewcerts
    #valid argument print out other valid arguments
    elif [ "$1" == "-h" ] || [ "$1" == "-help" ]; then
        echo ""
        echo "\"no argument\"        will attempt to update all certificates"
        echo "--override-ntru      updates all certificates except ntru"
        echo "-h or -help          display this menu"
        echo ""
        echo ""
    #else the argument was invalid, tell user to use -h or -help
    else
        echo ""
        echo "That is not a valid option."
        echo ""
        echo "use -h or -help for a list of available options."
        echo ""
    fi
#else HAVE_NTRU not already defined
else
    echo "Saving the configure state"
    echo ""
    cp config.status tmp.status
    cp wolfssl/options.h tmp.options.h

    echo "Running make clean"
    echo ""
    make clean

    #attempt to define ntru by configuring with ntru
    echo "Configuring with ntru, enabling certgen and keygen"
    echo ""
    ./configure --with-ntru --enable-certgen --enable-keygen
    make check

    # check options.h a second time, if the user had
    # ntru installed on their system and in the default
    # path location, then it will now be defined, if the
    # user does not have ntru on their system this will fail
    # again and we will not update any certs until user installs
    # ntru in the default location

    # if now defined
    if grep HAVE_NTRU "wolfssl/options.h"; then
        run_renewcerts
        #run_renewcerts leaves us in wolfssl/certs/crl, backup to root
        cd ../../
        echo "changed directory to wolfssl root directory."
        echo ""

        move_ntru

        echo "ntru-certs, and ntru-key.raw have been updated"
        echo ""

        # restore previous configure state
        restore_config
    else

        # restore previous configure state
        restore_config

        echo ""
        echo "ntru is not installed at the default location,"
        echo "or ntru not installed, none of the certs were updated."
        echo ""
        echo "clone the ntru repository into your \"cd ~\" directory then,"
        echo "\"cd NTRUEncrypt\" and run \"make\" then \"make install\""
        echo "once complete run this script again to update all the certs."
        echo ""
        echo "To update all certs except ntru use \"./renewcerts.sh --override-ntru\""
        echo ""

    fi #END now defined
fi #END already defined

