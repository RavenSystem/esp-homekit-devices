#!/bin/sh

# google.test

server=www.google.com

[ ! -x ./examples/client/client ] && echo -e "\n\nClient doesn't exist" && exit 1

# is our desired server there?
./scripts/ping.test $server 2
RESULT=$?
[ $RESULT -ne 0 ] && exit 0

# client test against the server
./examples/client/client -X -C -h $server -p 443 -g -d
RESULT=$?
[ $RESULT -ne 0 ] && echo -e "\n\nClient connection failed" && exit 1

exit 0
