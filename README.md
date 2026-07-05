# DIY OS
DIY OS based on MINT64 — a 64-bit multicore OS that boots from a floppy image
and runs under QEMU.

## Prerequisites (Windows)

The build runs under **Cygwin** and targets bare-metal x86_64 with a cross
toolchain. The following must be installed:

### 1. Cygwin + build tools
Install [Cygwin](https://www.cygwin.com/) to `C:\cygwin64`, and during setup
select these packages:
- `make`
- `nasm`
- `gdb`     (needed for debugging)
- `binutils`, `gcc-core` (host tools)

After install, `C:\cygwin64\bin` should contain `make.exe`, `nasm.exe`, `gdb.exe`.

### 2. x86_64 cross toolchain
A cross toolchain targeting `x86_64-pc-linux` is expected at
`C:\cygwin64\usr\cross\bin` (used with `-ffreestanding` to build the kernel):
- `x86_64-pc-linux-gcc.exe`
- `x86_64-pc-linux-ld.exe`
- `x86_64-pc-linux-objcopy.exe`

If not present, build a cross binutils + gcc for target `x86_64-pc-linux` and
install it there (or adjust the tool prefixes in `01.Kernel32/makefile` and
`02.Kernel64/makefile`).

### 3. QEMU
- **Bundled QEMU 0.10.4** (`qemu-0.10.4/`) — used for plain *running*.
- **Modern QEMU** — required for *debugging* (the 0.10.4 gdb stub is not
  compatible with modern gdb). Install with winget:
  ```
  winget install --id SoftwareFreedomConservancy.QEMU
  ```
  Expected at `C:\Program Files\qemu\qemu-system-x86_64.exe`.

### 4. VS Code extension
- **C/C++** (`ms-vscode.cpptools`) — required for the debugger:
  ```
  code --install-extension ms-vscode.cpptools
  ```

## Build
Make sure `C:\cygwin64\bin` and `C:\cygwin64\usr\cross\bin` are on `PATH`, then:
```
make            # builds Bootloader + Kernel32 + Kernel64 + Utility + Disk.img
make clean      # removes build outputs
```
The build produces `Disk.img`, padded to a standard 1.44MB floppy (1474560
bytes) so modern QEMU derives the correct geometry.

In VS Code: open `diy_os_develop.code-workspace`, then **Ctrl+Shift+B**.

## Run
```
qemu-0.10.4/qemu-x86_64.bat
```
or in VS Code: **Run Task → `run (QEMU)`** (builds first, then boots in the
bundled QEMU 0.10.4).

## Debug (real-time, source-level)
In VS Code press **F5** (`Debug Kernel64 (QEMU gdb stub)`). This builds, launches
modern QEMU with the gdb stub, attaches gdb, and stops at `main`. Use
breakpoints / step (F10/F11) / variable & register inspection as usual.

Debug notes:
- QEMU runs with `-S -gdb tcp:127.0.0.1:1234`; gdb attaches with `set osabi none`
  (required so cygwin gdb does not abort on the Windows-only qGetTIBAddr packet).
- Symbols come from `02.Kernel64/temp/Kernel64.elf` (kernel linked at 0x200000);
  the kernel is built with `-g -gdwarf-4` for source-line debugging.
- Execution stops at `main` on launch (stopAtEntry). Modern QEMU tracks software
  breakpoints by address, so normal VS Code breakpoints work in kernel code too.
