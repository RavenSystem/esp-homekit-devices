#!/bin/sh
#
#
# Our "pre-commit" hook.

# save current config
echo "\n\nSaving current config\n\n"
cp config.status tmp.status
cp wolfssl/options.h tmp.options.h 

# stash modified files not part of this commit, don't test them
echo "\n\nStashing any modified files not part of commit\n\n"
git stash -q --keep-index

# do the commit tests
echo "\n\nRunning commit tests...\n\n"
./commit-tests.sh
RESULT=$?

# restore modified files not part of this commit
echo "\n\nPopping any stashed modified files not part of commit\n"
git stash pop -q

# restore current config
echo "\nRestoring current config\n"
mv tmp.status config.status
# don't show output incase error from above
./config.status >/dev/null 2>&1
mv tmp.options.h wolfssl/options.h 
make clean >/dev/null 2>&1
make -j 8 >/dev/null 2>&1

[ $RESULT -ne 0 ] && echo "\nOops, your commit failed\n" && exit 1

echo "\nCommit tests passed!\n"
exit 0
