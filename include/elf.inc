; IKOS ELF Format Include File
; Assembly constants for ELF kernel loading

; ELF Magic Numbers
%define ELF_MAGIC_0         0x7F
%define ELF_MAGIC_1         'E'
%define ELF_MAGIC_2         'L'
%define ELF_MAGIC_3         'F'

; ELF Class
%define ELF_CLASS_32        1
%define ELF_CLASS_64        2

; ELF Data Encoding
%define ELF_DATA_LSB        1
%define ELF_DATA_MSB        2

; ELF File Types
%define ELF_TYPE_EXEC       2               ; Executable file

; ELF Machine Types
%define ELF_MACHINE_386     3               ; Intel 80386

; Program Header Types
%define PT_NULL             0               ; Unused entry
%define PT_LOAD             1               ; Loadable segment

; Program Header Flags
%define PF_X                0x1             ; Execute permission
%define PF_W                0x2             ; Write permission
%define PF_R                0x4             ; Read permission

; ELF Header Offsets (32-bit)
%define ELF_E_IDENT         0               ; ELF identification (16 bytes)
%define ELF_E_TYPE          16              ; Object file type (2 bytes)
%define ELF_E_MACHINE       18              ; Architecture (2 bytes)
%define ELF_E_VERSION       20              ; Object file version (4 bytes)
%define ELF_E_ENTRY         24              ; Entry point address (4 bytes)
%define ELF_E_PHOFF         28              ; Program header offset (4 bytes)
%define ELF_E_SHOFF         32              ; Section header offset (4 bytes)
%define ELF_E_FLAGS         36              ; Processor flags (4 bytes)
%define ELF_E_EHSIZE        40              ; ELF header size (2 bytes)
%define ELF_E_PHENTSIZE     42              ; Program header entry size (2 bytes)
%define ELF_E_PHNUM         44              ; Program header entry count (2 bytes)

; Program Header Offsets (32-bit)
%define ELF_P_TYPE          0               ; Segment type (4 bytes)
%define ELF_P_OFFSET        4               ; File offset (4 bytes)
%define ELF_P_VADDR         8               ; Virtual address (4 bytes)
%define ELF_P_PADDR         12              ; Physical address (4 bytes)
%define ELF_P_FILESZ        16              ; File size (4 bytes)
%define ELF_P_MEMSZ         20              ; Memory size (4 bytes)
%define ELF_P_FLAGS         24              ; Flags (4 bytes)
%define ELF_P_ALIGN         28              ; Alignment (4 bytes)

; Size Constants
%define ELF_HEADER_SIZE     52              ; Size of ELF header
%define PROGRAM_HEADER_SIZE 32              ; Size of program header entry

; Kernel Loading Constants
%define KERNEL_LOAD_ADDR    0x100000        ; 1MB kernel load address
%define KERNEL_BUFFER       0x10000         ; 64KB temporary buffer at 64KB
%define DISK_BUFFER         0x20000         ; 128KB disk read buffer
%define KERNEL_START_SECTOR 2               ; Kernel starts at sector 2

; Disk Parameters
%define SECTORS_PER_TRACK   18              ; Floppy disk sectors per track
%define HEADS_PER_CYLINDER  2               ; Floppy disk heads
%define BYTES_PER_SECTOR    512             ; Standard sector size
