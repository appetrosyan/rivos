
## Warning: this project doesn't actually have a readme yet. I don't believe in creating such things before there is more substance to the project. a-p-petrosyan insisted however on renaming the rivos_manjaro_guide as a temporary readme.

# rivos manjaro guide
```

 # First clone the repository
git clone https://github.com/samhsmith/rivos.git
cd rivos
git submodule update --init --recursive
cd ..

 # Clone the risc-v gnu toolchain
git clone https://github.com/riscv/riscv-gnu-toolchain.git

 # Install dependancies required for build
sudo pacman -Sy autoconf automake curl python3 mpc mpfr gmp gawk base-devel bison flex texinfo gperf libtool patchutils bc zlib expat

 # Build the toolchain and install to /opt/riscv/ this will take a while
cd riscv-gnu-toolchain
sudo mkdir /opt/riscv
PATH=/opt/riscv/bin:$PATH
./configure --prefix=/opt/riscv && sudo make
cd ..

 # Now install the dependancies of running rivos
sudo pacman -Sy ninja ceph glusterfs libiscsi python python-sphinx spice-protocol xfsprogs

 # Now build the virtual machine that is used to run rivos
cd rivos
mkdir qemu/build
cd qemu/build
../configure --target-list=riscv64-softmmu
make -j$(nproc)
cd ../..

 # To run the virtual machine
make run

 # By default you're gonna get my super gnarly ultra custom keyboard layout that you probably
 # don't want to use.
 # To switch to sensible US Qwerty do the following:
 # In src/tempuser.h do the following change.

sam's custom layout:
#include "samorak.h"
//#include "qwerty.h"

US Qwerty:
//#include "samorak.h"
#include "qwerty.h"

