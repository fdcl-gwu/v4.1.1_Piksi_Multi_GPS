# System Libraries needed to install:
## LibSerialPort
# LibSerialPort Installation Ubuntu:
## `sudo apt install libserialport-dev`


# Libraries needed to install as git submodules:
## Zenoh-cpp
## Libsbp v4.1.1

## Update these using
### `git submodule update --init`

### `git submodule sync`
Update project configuration with the new information.

### `mkdir build && cd build`

### `cmake -G Ninja ..`













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

