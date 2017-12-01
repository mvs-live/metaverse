![logo](https://github.com/mvs-org/metaverse/raw/master/doc/image/logo.png)
Integration/staging tree
=========================

# Introduction
Metaverse(MVS) is a decentralised system based on the blockchain technology, through which, a network of smart properties, digital identities and value intermediators are established.

**Metaverse on Blockchain Development Path**:

![dev-path](https://github.com/mvs-org/metaverse/raw/master/doc/image/dev-path.jpg)

**Metaverse Features**:
- Digital Assets Register/Transfer/Order
- Digital Assets Exchange
- Digital Identities
- Oralces And Data-feed

# MVS Project
MVS is implemented based on [libbitcoin project](https://github.com/libbitcoin).

Further Read: [Wiki Documents](https://github.com/mvs-org/metaverse/wiki)

# build MVS
## toolchain requirements:
- C++ compiler support C++14 (g++ 5/LLVM 8.0.0/MSVC14)
- CMake 2.8+

```bash
git clone https://github.com/mvs-org/metaverse.git
cd metaverse && mkdir build && cd build
cmake ..
make -j4
make install
```
optional:
```
make test
make doc
```

# Library Dependencies
## boost 1.56+
```bash
sudo yum/apt-get/brew install libboost-all-dev
```
If build boost manually, please download boost from <http://www.boost.org/>.

If build with boost 1.59~1.63, get compiling error on json_parser 'placeholders::_1' caused by boost bug:
```
/usr/local/include/boost/property_tree/json_parser/detail/parser.hpp:217:52: error: ‘_1’ was not declared in this scope
```
Please upgrade to 1.64, or modify parser.hpp manually at first.
See boost issue details: <https://github.com/boostorg/property_tree/pull/26>

## ZeroMQ 4.2.1+
Install GNU toochain(automake/autoconf/libtool) at first:
```bash
yum/apt-get/brew install automake/autoconf/libtool
```
Module server/explorer required.
```bash
wget https://github.com/zeromq/libzmq/releases/download/v4.2.1/zeromq-4.2.1.tar.gz
tar -xzvf zeromq-4.2.1.tar.gz
cd zeromq-4.2.1
./autogen.sh
./configure
make -j4
sudo make install && sudo ldconfig
```

## secp256k1 
Module blockchain/database required.
```bash
git clone https://github.com/mvs-live/secp256k1
cd secp256k1
./autogen.sh
./configure --enable-module-recovery
make -j4
sudo make install && sudo ldconfig
```
