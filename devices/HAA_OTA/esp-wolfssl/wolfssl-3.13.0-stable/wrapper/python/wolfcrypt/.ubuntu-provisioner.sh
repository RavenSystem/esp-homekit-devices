[ "$(whoami)" != "root" ] && echo "Sorry, you are not root." && exit 1

apt-get update
apt-get install -y git autoconf libtool

git clone https://github.com/wolfssl/wolfssl.git
[ $? -ne 0 ] && echo "\n\nCouldn't download wolfssl.\n\n" && exit 1

pushd wolfssl

./autogen.sh
./configure
make
make install
ldconfig

popd
rm -rf wolfssl

apt-get install -y libffi-dev python-dev python-pip

pip install wolfcrypt
[ $? -ne 0 ] && echo "\n\nCouldn't install wolfcrypt.\n\n" && exit 1

echo "Test should print:"
echo "da39a3ee5e6b4b0d3255bfef95601890afd80709"
echo "Running test:"
python -c "from wolfcrypt.hashes import Sha; print(Sha().hexdigest())"
