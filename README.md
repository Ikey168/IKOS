# Custom Microkernel-Based OS

## About
This project is a **general-purpose microkernel-based operating system** designed for **x86 and x86_64 consumer devices** (desktops, laptops). It features a **custom bootloader**, a **virtual memory manager (VMM)**, and **message-passing-based inter-process communication (IPC)**. The OS is developed with a **modular and parallel development approach**, allowing independent system components to evolve without a strict module API. The security model is **permissive**, prioritizing user control and ease of development.

## Features
- **Custom Bootloader**: Initializes CPU segments and loads the kernel. Real mode setup and BIOS memory map retrieval live in `boot/real_mode.asm` with entries stored at `0x0500`.
- **Microkernel Architecture**: Only core functions in the kernel; drivers and services in user space.
- **Virtual Memory Management (VMM)**: Paging-based memory isolation.
- **Message-Passing IPC**: Inter-process communication via queues.
- **Parallel Feature Development**: No strict module API for flexibility.
- **Basic Device Support**: Keyboard, display, disk, networking.
- **Preemptive Multi-tasking**: Round-robin or priority scheduling.
- **Filesystem Support**: Virtual file system (VFS), with initial FAT support.
- **Graphical UI (Planned)**: Framebuffer-based window system.
- **Networking Stack (Planned)**: Basic TCP/IP for internet access.

## Architecture Overview
```
+---------------------------------------------------+
| User Applications (GUI, Shell, Web, Filesystem)  |
+---------------------------------------------------+
| System Services (FS, Network, Audio, etc.)       |
+---------------------------------------------------+
| Device Drivers (GPU, Storage, Input, etc.)       |
+---------------------------------------------------+
| Microkernel (IPC, Scheduler, VMM, Interrupts)    |
+---------------------------------------------------+
| Hardware (CPU, RAM, Disk, GPU, Network, etc.)    |
+---------------------------------------------------+
```

## Getting Started
### Prerequisites
- **x86-64 Cross Compiler** (GCC, Clang, or another suitable toolchain)
- **QEMU** (for emulation and testing)
- **GRUB** (if using a GRUB-compatible bootloader)
- **Make, NASM, and C/C++ Compiler**

### Building the OS
1. Clone the repository:
   ```sh
   git clone https://github.com/yourusername/custom-os.git
   cd custom-os
   ```
2. Compile the bootloader and kernel:
   ```sh
   nasm -f bin boot/real_mode.asm -o boot/real_mode.bin  # assemble real-mode boot sector
   make all  # build other components
   ```
3. Run in QEMU:
   ```sh
   make run
   ```

### Testing
- Use `make test` to run unit tests on kernel components.
- Debugging can be done using `GDB` with QEMU:
   ```sh
   make debug
   ```

## Roadmap
- [x] Bootloader Development
- [x] Microkernel Core (IPC, Scheduling, VMM)
- [ ] User-space Services (Filesystem, Networking, Device Drivers)
- [ ] GUI & Shell Development
- [ ] Networking Stack & Advanced Features

## Contributing
Contributions are welcome! Please follow these steps:
1. Fork the repository.
2. Create a new branch (`feature-name`).
3. Commit changes (`git commit -m "Added feature X"`).
4. Push to the branch (`git push origin feature-name`).
5. Open a pull request.

## License
This project is licensed under the MIT License. See `LICENSE` for details.


