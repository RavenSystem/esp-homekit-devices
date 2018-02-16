#!/bin/sh

#tls-cert-fail.test

asn_no_signer_e="-188"
asn_sig_confirm_e="-155"
exit_code=1
counter=0

# need a unique resume port since may run the same time as testsuite
# use server port zero hack to get one
tls_port=0

#no_pid tells us process was never started if -1
no_pid=-1

#server_pid captured on startup, stores the id of the server process
server_pid=$no_pid

# let's use absolute path to a local dir (make distcheck may be in sub dir)
# also let's add some randomness by adding pid in case multiple 'make check's
# per source tree
ready_file=`pwd`/wolfssl_tls_ready$$

remove_ready_file() {
    if test -e $ready_file; then
        echo -e "removing existing ready file"
        rm $ready_file
    fi
}

# trap this function so if user aborts with ^C or other kill signal we still
# get an exit that will in turn clean up the file system
abort_trap() {
    echo "script aborted"

    if  [ $server_pid != $no_pid ]
    then
        echo "killing server"
        kill -9 $server_pid
    fi

    exit_code=2 #different exit code in case of user interrupt

    echo "got abort signal, exiting with $exit_code"
    exit $exit_code
}
trap abort_trap INT TERM


# trap this function so that if we exit on an error the file system will still
# be restored and the other tests may still pass. Never call this function
# instead use "exit <some value>" and this function will run automatically
restore_file_system() {
    remove_ready_file
}
trap restore_file_system EXIT

run_tls_no_signer_test() {
    echo -e "\nStarting example server for tls no signer fail test...\n"

    remove_ready_file

    # starts the server on tls_port, -R generates ready file to be used as a
    # mutex lock. We capture the processid into the variable server_pid
    ./examples/server/server -R $ready_file -p $tls_port &
    server_pid=$!

    while [ ! -s $ready_file -a "$counter" -lt 20 ]; do
        echo -e "waiting for ready file..."
        sleep 0.1
        counter=$((counter+ 1))
    done

    if test -e $ready_file; then
        echo -e "found ready file, starting client..."
    else
        echo -e "NO ready file ending test..."
        exit 1
    fi

    # get created port 0 ephemeral port
    tls_port=`cat $ready_file`

    # starts client on tls_port and captures the output from client
    capture_out=$(./examples/client/client -p $tls_port -H badCert 2>&1)
    client_result=$?

    wait $server_pid
    server_result=$?

    case  "$capture_out" in
    *$asn_no_signer_e*)
        # only exit with zero on detection of the expected error code
        echo ""
        echo "$capture_out"
        echo ""
        echo "No signer error as expected! Test pass"
        echo ""
        exit_code=0
        ;;
    *)
        echo ""
        echo "Client did not return asn_no_signer_e as expected: $capture_out"
        echo ""
        exit_code=1
    esac
}

run_tls_sig_confirm_test() {
    echo -e "\nStarting example server for tls sig confirm fail test...\n"

    remove_ready_file

    # starts the server on tls_port, -R generates ready file to be used as a
    # mutex lock. We capture the processid into the variable server_pid
    ./examples/server/server -R $ready_file -p $tls_port -H badCert &
    server_pid=$!

    while [ ! -s $ready_file -a "$counter" -lt 20 ]; do
        echo -e "waiting for ready file..."
        sleep 0.1
        counter=$((counter+ 1))
    done

    if test -e $ready_file; then
        echo -e "found ready file, starting client..."
    else
        echo -e "NO ready file ending test..."
        exit 1
    fi

    # get created port 0 ephemeral port
    tls_port=`cat $ready_file`

    # starts client on tls_port and captures the output from client
    capture_out=$(./examples/client/client -p $tls_port 2>&1)
    client_result=$?

    wait $server_pid
    server_result=$?

    case  "$capture_out" in
    *$asn_sig_confirm_e*)
        # only exit with zero on detection of the expected error code
        echo ""
        echo "$capture_out"
        echo ""
        echo "Sig confirm error as expected! Test pass"
        echo ""
        exit_code=0
        ;;
    *)
        echo ""
        echo "Client did not return asn_sig_confirm_e as expected: $capture_out"
        echo ""
        exit_code=1
    esac
}


######### begin program #########

# run the test
run_tls_no_signer_test

tls_port=0
run_tls_sig_confirm_test

echo "exiting with $exit_code"
exit $exit_code
########## end program ##########

