# Project Overview

This project contains QCAP Video I/O kernel modules and user-space sample applications. The kernel modules appear to be designed for Xilinx Zynq MPSoC devices and provide video capture and processing capabilities. The project is structured into two main directories: `modules` and `samples`.

*   `modules`: This directory contains the source code for the kernel modules, including `qdmabuf` and `qvio-l4t`. These modules seem to handle DMA buffer management and video I/O operations.
*   `samples`: This directory contains a collection of user-space applications that demonstrate how to interact with the kernel modules. These samples cover functionalities like V4L control, qdmabuf control, and other qvio-related operations.

# Building and Running

The project uses a recursive Make-based build system. To build the entire project, including the kernel modules and sample applications, run the following command in the root directory:

```bash
make
```

To clean the build artifacts, you can use:

```bash
make clean
```

The build process is defined in the top-level `Makefile` and the individual `Makefile`s within each module and sample directory.

# Development Conventions

*   **Modular Structure:** The project is organized into independent modules and samples, each with its own `Makefile`. This modular design allows for building and maintaining different components separately.
*   **Kernel Development:** The kernel modules follow the standard Linux kernel module development practices. The `Makefile`s in the module directories are typical for building kernel modules.
*   **User-space Applications:** The sample applications are written in C++ and provide examples of how to use the kernel modules from user space.
*   **Vitis HLS:** The project seems to integrate with Vitis HLS, as indicated by the presence of `vitis` directory and related source files.
