[ "$(whoami)" != "root" ] && echo "Sorry, you are not root." && exit 1

rpm -ivh http://dl.fedoraproject.org/pub/epel/7/x86_64/e/epel-release-7-6.noarch.rpm
yum update
yum install -y git autoconf libtool

git clone https://github.com/wolfssl/wolfssl.git
[ $? -ne 0 ] && echo "\n\nCouldn't download wolfssl.\n\n" && exit 1

pushd wolfssl

./autogen.sh
./configure
make
make install
echo /usr/local/lib > wolfssl.conf
mv wolfssl.conf /etc/ld.so.conf
ldconfig

popd
rm -rf wolfssl

yum install -y libffi-devel python-devel python-pip

pip install wolfcrypt
[ $? -ne 0 ] && echo "\n\nCouldn't install wolfcrypt.\n\n" && exit 1

echo "Test should print:"
echo "da39a3ee5e6b4b0d3255bfef95601890afd80709"
echo "Running test:"
python -c "from wolfcrypt.hashes import Sha; print(Sha().hexdigest())"
