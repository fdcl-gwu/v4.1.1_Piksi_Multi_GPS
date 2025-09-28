# System libraries needed to install:

## Zenoh-cpp
## Libsbp v4.1.1

## Update these using
## `git submodule update --init`
This registers the submodule to the correct path, contained in .gitmodules

## `git submodule sync`
Update project configuration with the new information.

## `git submodule update --init`
Pull the source code from the repo

# Installation of LibSBP:
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
sudo ninja install?

