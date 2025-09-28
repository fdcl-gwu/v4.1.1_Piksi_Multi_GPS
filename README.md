# System libraries needed to install:

## Zenoh-cpp
## Libsbp v4.1.1

## Update these using
## `git submodule update --init`

Both are contained in this program, 

Installation of LibSBP:
cd /libsbp/c/
git submodule update --init
cd build
cmake -G 'Ninja' ../
ninja
sudo ninja install


Installation of zenoh-c

install current version of zenoh-c from github
install latest version of rustup
Install to system with ninja


