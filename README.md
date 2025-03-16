# IKOS - Innovative Kernel Operating System

**IKOS** (Innovative Kernel Operating System) is a modular **microkernel-based** operating system built from scratch in **C++** for **x86 and x86_64** architectures. Designed for flexibility, modularity, and security, IKOS enables independent development of system components, user-space drivers, and dynamic service management.

## 🚀 Features

- **Microkernel Architecture** – Minimal kernel, most services run in user space.
- **POSIX Compatibility** – Enables porting of existing software.
- **User-Space Drivers** – Device drivers operate as modular, isolated services.
- **Inter-Process Communication (IPC)** – Secure message-based communication.
- **Dynamic Module Loading** – Hot-swappable kernel and user-space components.
- **Virtual Memory & Paging** – Efficient memory management.
- **Networking Support** – TCP/IP networking stack (WIP).
- **Graphical UI (Planned)** – GUI-based desktop environment and window manager.

---

## 📌 Current Development Milestones

### ✅ **Milestone 1: Bootstrapping the Kernel** (🟢 Ongoing)
- Kernel initialization, memory management, and task scheduling.
- Booting via GRUB with basic VGA text output.

### 🔜 **Upcoming Milestones:**
1. **Process & Memory Management** – Implement process control, syscalls, and IPC.
2. **User-Space Services** – Develop a CLI shell, ELF loader, and virtual filesystem.
3. **Modular Drivers** – Implement user-space keyboard, disk, and serial drivers.
4. **Networking Stack** – Develop a TCP/IP networking system.
5. **GUI System** – Implement a window manager and graphical user interface.
6. **Security & Stability Enhancements** – Introduce sandboxing, permissions, and crash handling.

---

## 🛠️ Getting Started

### 🔧 **Build Requirements**
- **OS:** Linux/macOS (recommended for development)
- **Compiler:** `x86_64-elf-gcc`
- **Emulator:** QEMU / Bochs
- **Build System:** Make / CMake

### 🏗 **Building IKOS**

```sh
# Clone the repository
git clone https://github.com/your-username/IKOS.git
cd IKOS

# Setup cross compiler (if not installed)
make toolchain

# Build the OS
make

# Run in QEMU
make run
```

### 🏁 **Running IKOS in QEMU**

To test IKOS in QEMU:
```sh
make run
```

To debug using GDB:
```sh
make debug
```

---

## 📜 License
*(Specify your open-source license, e.g., MIT, GPLv2, or custom.)*

---

## 🤝 Contributing
Contributions are welcome! If you’d like to help:

1. Fork the repository and create a feature branch.
2. Commit changes with descriptive messages.
3. Open a pull request with details of the proposed changes.

For major changes, please open an issue to discuss before implementing.

---

## 📢 Community & Support
- **Discussions:** *(Link to GitHub Discussions or Discord server)*
- **Issues:** *(Link to issue tracker for bug reports and feature requests)*
- **Documentation:** *(Link to additional docs, if available)*

---

### 🚀 Join us in building the future of modular operating systems!

