#!/bin/sh

# tls13.test
# copyright wolfSSL 2016

# getting unique port is modeled after resume.test script
# need a unique port since may run the same time as testsuite
# use server port zero hack to get one
port=0
no_pid=-1
server_pid=$no_pid
counter=0
# let's use absolute path to a local dir (make distcheck may be in sub dir)
# also let's add some randomness by adding pid in case multiple 'make check's
# per source tree
ready_file=`pwd`/wolfssl_psk_ready$$

echo "ready file $ready_file"

create_port() {
    while [ ! -s $ready_file -a "$counter" -lt 50 ]; do
        echo -e "waiting for ready file..."
        sleep 0.1
        counter=$((counter+ 1))
    done

    if test -e $ready_file; then
        echo -e "found ready file, starting client..."

        # get created port 0 ephemeral port
        port=`cat $ready_file`
    else
        echo -e "NO ready file ending test..."
        do_cleanup
    fi
}

remove_ready_file() {
    if test -e $ready_file; then
        echo -e "removing existing ready file"
    rm $ready_file
    fi
}

do_cleanup() {
    echo "in cleanup"

    if  [ $server_pid != $no_pid ]
    then
        echo "killing server"
        kill -9 $server_pid
    fi
    remove_ready_file
}

do_trap() {
    echo "got trap"
    do_cleanup
    exit -1
}

trap do_trap INT TERM

[ ! -x ./examples/client/client ] && echo -e "\n\nClient doesn't exist" && exit 1

# Usual TLS v1.3 server / TLS v1.3 client.
echo -e "\n\nTLS v1.3 server with TLS v1.3 client"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 not enabled"
    do_cleanup
    exit 1
fi
echo ""

# Usual TLS v1.3 server / TLS v1.3 client - fragment.
echo -e "\n\nTLS v1.3 server with TLS v1.3 client - fragment"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -F 1 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 and fragments not working"
    do_cleanup
    exit 1
fi
echo ""

# Use HelloRetryRequest with TLS v1.3 server / TLS v1.3 client.
echo -e "\n\nTLS v1.3 HelloRetryRequest"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -J -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 HelloRetryRequest not working"
    do_cleanup
    exit 1
fi
echo ""

# Use HelloRetryRequest with TLS v1.3 server / TLS v1.3 client using cookie
echo -e "\n\nTLS v1.3 HelloRetryRequest with cookie"
port=0
./examples/server/server -v 4 -J -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -J -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 HelloRetryRequest with cookie not working"
    do_cleanup
    exit 1
fi
echo ""

# Use HelloRetryRequest with TLS v1.3 server / TLS v1.3 client - SHA384.
echo -e "\n\nTLS v1.3 HelloRetryRequest - SHA384"
port=0
./examples/server/server -v 4 -l TLS13-AES256-GCM-SHA384 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -J -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 HelloRetryRequest with SHA384 not working"
    do_cleanup
    exit 1
fi
echo ""

# Resumption TLS v1.3 server / TLS v1.3 client.
echo -e "\n\nTLS v1.3 resumption"
port=0
./examples/server/server -v 4 -r -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -r -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 resumption not working"
    do_cleanup
    exit 1
fi
echo ""

# Resumption TLS v1.3 server / TLS v1.3 client - SHA384
echo -e "\n\nTLS v1.3 resumption - SHA384"
port=0
./examples/server/server -v 4 -l TLS13-AES256-GCM-SHA384 -r -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -l TLS13-AES256-GCM-SHA384 -r -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 resumption with SHA384 not working"
    do_cleanup
    exit 1
fi
echo ""

./examples/client/client -v 4 -e 2>&1 | grep -- '-ECC'
if [ $? -eq 0 ]; then
    # Usual TLS v1.3 server / TLS v1.3 client and ECC certificates.
    echo -e "\n\nTLS v1.3 server with TLS v1.3 client - ECC certificates"
    port=0
    ./examples/server/server -v 4 -A certs/client-ecc-cert.pem -c certs/server-ecc.pem -k certs/ecc-key.pem -R $ready_file -p $port &
    server_pid=$!
    create_port
    ./examples/client/client -v 4 -A certs/ca-ecc-cert.pem -c certs/client-ecc-cert.pem -k certs/ecc-client-key.pem -p $port
    RESULT=$?
    remove_ready_file
    if [ $RESULT -ne 0 ]; then
        echo -e "\n\nTLS v1.3 ECC certificates not working"
        do_cleanup
        exit 1
    fi
    echo ""
fi

# Usual TLS v1.3 server / TLS v1.3 client and no client certificate.
echo -e "\n\nTLS v1.3 server with TLS v1.3 client - no client cretificate"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -x -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 and no client certificate not working"
    do_cleanup
    exit 1
fi
echo ""

# Usual TLS v1.3 server / TLS v1.3 client and DH Key.
echo -e "\n\nTLS v1.3 server with TLS v1.3 client - DH Key Exchange"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -y -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 DH Key Exchange not working"
    do_cleanup
    exit 1
fi
echo ""

# Usual TLS v1.3 server / TLS v1.3 client and ECC Key.
echo -e "\n\nTLS v1.3 server with TLS v1.3 client - ECC Key Exchange"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -Y -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nTLS v1.3 ECDH Key Exchange not working"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 cipher suites server / client.
echo -e "\n\nOnly TLS v1.3 cipher suites"
port=0
./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-AES128-GCM-SHA256:TLS13-AES256-GCM-SHA384:TLS13-CHACHA20-POLY1305-SHA256:TLS13-AES128-CCM-SHA256:TLS13-AES128-CCM-8-SHA256 &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 cipher suites - only TLS v1.3"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 cipher suites server / client.
echo -e "\n\nOnly TLS v1.3 cipher suite - AES128-GCM SHA-256"
port=0
./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-AES128-GCM-SHA256 &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 cipher suites - AES128-GCM SHA-256"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 cipher suites server / client.
echo -e "\n\nOnly TLS v1.3 cipher suite - AES256-GCM SHA-384"
port=0
./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-AES256-GCM-SHA384 &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 cipher suites - AES256-GCM SHA-384"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 cipher suites server / client.
echo -e "\n\nOnly TLS v1.3 cipher suite - CHACHA20-POLY1305 SHA-256"
port=0
./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-CHACHA20-POLY1305-SHA256 &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 cipher suites - CHACHA20-POLY1305 SHA-256"
    do_cleanup
    exit 1
fi
echo ""

./examples/client/client -v 4 -e 2>&1 | grep -- '-CCM'
if [ $? -eq 0 ]; then
    # TLS 1.3 cipher suites server / client.
    echo -e "\n\nOnly TLS v1.3 cipher suite - AES128-CCM SHA-256"
    port=0
    ./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-AES128-CCM-SHA256 &
    server_pid=$!
    create_port
    ./examples/client/client -v 4 -p $port
    RESULT=$?
    remove_ready_file
    if [ $RESULT -ne 0 ]; then
        echo -e "\n\nIssue with TLS v1.3 cipher suites - AES128-CCM SHA-256"
        do_cleanup
        exit 1
    fi
    echo ""

    # TLS 1.3 cipher suites server / client.
    echo -e "\n\nOnly TLS v1.3 cipher suite - AES128-CCM-8 SHA-256"
    port=0
    ./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-AES128-CCM-8-SHA256 &
    server_pid=$!
    create_port
    ./examples/client/client -v 4 -p $port
    RESULT=$?
    remove_ready_file
    if [ $RESULT -ne 0 ]; then
        echo -e "\n\nIssue with TLS v1.3 cipher suites - AES128-CCM-8 SHA-256"
        do_cleanup
        exit 1
    fi
    echo ""
fi

# TLS 1.3 cipher suites server / client.
echo -e "\n\nTLS v1.3 cipher suite mismatch"
port=0
./examples/server/server -v 4 -R $ready_file -p $port -l TLS13-CHACHA20-POLY1305-SHA256 &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port -l TLS13-AES256-GCM-SHA384
RESULT=$?
remove_ready_file
if [ $RESULT -ne 1 ]; then
    echo -e "\n\nIssue with mismatched TLS v1.3 cipher suites"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 server / TLS 1.2 client.
echo -e "\n\nTLS v1.3 server downgrading to TLS v1.2"
port=0
./examples/server/server -v 4 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 3 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -eq 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 server downgrading to TLS v1.2"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.2 server / TLS 1.3 client.
echo -e "\n\nTLS v1.3 client upgrading server to TLS v1.3"
port=0
./examples/server/server -v 3 -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -eq 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 client upgrading server to TLS v1.3"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 server / TLS 1.3 client send KeyUpdate before sending app data.
echo -e "\n\nTLS v1.3 KeyUpdate"
port=0
./examples/server/server -v 4 -U -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -I -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 KeyUpdate"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 server / TLS 1.3 client don't use (EC)DHE with PSK.
echo -e "\n\nTLS v1.3 KeyUpdate"
port=0
./examples/server/server -v 4 -r -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -r -K -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 KeyUpdate"
    do_cleanup
    exit 1
fi
echo ""

# TLS 1.3 server / TLS 1.3 client and Post-Handshake Authentication.
echo -e "\n\nTLS v1.3 Post-Handshake Authentication"
port=0
./examples/server/server -v 4 -Q -R $ready_file -p $port &
server_pid=$!
create_port
./examples/client/client -v 4 -Q -p $port
RESULT=$?
remove_ready_file
if [ $RESULT -ne 0 ]; then
    echo -e "\n\nIssue with TLS v1.3 Post-Handshake Auth"
    do_cleanup
    exit 1
fi
echo ""

echo -e "\nALL Tests Passed"

exit 0

