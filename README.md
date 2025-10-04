# Project Installation Guide

This guide outlines the necessary dependencies and steps to build the project on an Ubuntu-based system.

-----

## 1\. System Dependencies

Before compiling, you need to install a few system-wide libraries and build tools.

  * **CMake**: A build system generator.
  * **Ninja**: A small, fast build system used with CMake.
  * **LibSerialPort**: A library for handling serial port communication.

Open your terminal and run the following command to install them all:

```bash
sudo apt update && sudo apt install cmake ninja-build libserialport-dev
```

-----

## 2\. Git Submodules

This project relies on the following libraries, which are included as Git submodules:

  * **Zenoh-cpp**
  * **Libsbp v4.1.1**

-----

## 3\. Build Process

Follow these steps in your terminal from the root directory of the project.

1.  **Initialize and Update Submodules**
    This command downloads and checks out the correct versions of the submodule libraries.

    ```bash
    git submodule update --init
    ```

2.  **Configure the Build**
    This creates a `build` directory and runs CMake to generate the build files for Ninja.

    ```bash
    mkdir build && cd build
    cmake -G Ninja ..
    ```

3.  **Compile the Project**
    Run Ninja to compile the source code.

    ```bash
    ninja
    ```

After these steps are complete, the compiled binary will be available inside the `build` directory.
