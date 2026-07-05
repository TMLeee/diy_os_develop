# DIY OS
DIY OS based on MINT64

# RUN
-------------
Execute bat file
> qemu-0.10.4/qemu-x86_64.bat

# Build & Debug (VS Code)
-------------
Open `diy_os_develop.code-workspace` in VS Code.

- Build      : Ctrl+Shift+B  (task: `build`)  → generates Disk.img
- Run        : Run Task → `run (QEMU)`         → boots Disk.img in bundled QEMU 0.10.4
- Debug (F5) : `Debug Kernel64 (QEMU gdb stub)`→ real-time source-level debugging

Toolchain / requirements:
- cygwin make + nasm + x86_64 cross toolchain (C:\cygwin64)
- cygwin gdb (C:\cygwin64\bin\gdb.exe)
- VS Code C/C++ extension (ms-vscode.cpptools)
- Modern QEMU (C:\Program Files\qemu) — used only for debugging; the old 0.10.4
  gdb stub is not compatible with modern gdb.

Debug notes:
- QEMU runs with `-S -gdb tcp:127.0.0.1:1234`; gdb attaches with `set osabi none`
  (required so cygwin gdb does not abort on the Windows-only qGetTIBAddr packet).
- Symbols come from 02.Kernel64/temp/Kernel64.elf (kernel linked at 0x200000);
  the kernel is built with `-g -gdwarf-4` for source-line debugging.
- Execution stops at `main` on launch (stopAtEntry). Modern QEMU tracks software
  breakpoints by address, so normal VS Code breakpoints work in kernel code too.
