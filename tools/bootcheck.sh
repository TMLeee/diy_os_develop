#!/bin/bash
# bootcheck.sh - headless QEMU boot verification for 10.OS
#
# Boots Disk.img, optionally types one or more shell commands, dumps the VGA
# text buffer (0xB8000) via the QEMU monitor, and decodes it to plain text.
#
# Usage:
#   bootcheck.sh                                  # just boot, dump screen
#   bootcheck.sh "totalram"                       # boot, type it, dump
#   bootcheck.sh "totalram" "createtask 2 4"      # several commands in ONE boot
#
# Env:
#   BOOT_WAIT  seconds after power-on before typing        (default 5)
#   CMD_WAIT   seconds after each command                  (default 2)
#   END_WAIT   seconds after the last command before dump  (default 2)
#   MEM        QEMU -m value                               (default 64)
#   SERIAL     capture serial output to this file (absolute path)
#   INTLOG     set to 1 to enable -d int,guest_errors (SLOW; for triple faults)
#   KEEPSCREEN set to 1 to keep vga.bin

set -u

# Windows ships its own timeout.exe / find.exe etc; make the POSIX ones win.
export PATH="/usr/bin:/bin:$PATH"

# Git Bash mounts C: at /c, Cygwin at /cygdrive/c. cygpath exists in both and
# returns whichever form the running shell actually understands.
tounix() { cygpath -u "$1" 2>/dev/null || echo "$1"; }

ROOT="$(tounix 'C:/workspace/10.OS')"
QEMU="$(tounix 'C:/Program Files/qemu/qemu-system-x86_64.exe')"
WORK="$(cd "$(dirname "$0")" && pwd)"          # always absolute
VGA_BIN="$WORK/vga.bin"
QLOG="$WORK/qemu.log"

BOOT_WAIT="${BOOT_WAIT:-5}"
CMD_WAIT="${CMD_WAIT:-2}"
END_WAIT="${END_WAIT:-2}"
MEM="${MEM:-64}"

towin() { cygpath -m "$1" 2>/dev/null || echo "$1"; }

rm -f "$VGA_BIN" "$QLOG"
VGA_WIN="$(towin "$VGA_BIN")"

# QEMU is a native Windows binary: every path handed to it must be Windows-form.
qargs=(-display none -monitor stdio
       -m "$MEM" -fda "$(towin "$ROOT/Disk.img")" -boot a -M pc
       -rtc base=localtime -no-reboot)

if [ -n "${SERIAL:-}" ]; then
    rm -f "$SERIAL"
    qargs+=(-serial "file:$(towin "$SERIAL")")
fi
if [ "${INTLOG:-0}" = "1" ]; then
    qargs+=(-d int,guest_errors -D "$(towin "$QLOG")")
fi

# Hard watchdog so a wedged guest can never hang the harness. Must be the POSIX
# timeout, not Windows' TIMEOUT.EXE.
if [ -x /usr/bin/timeout ]; then
    TO=(/usr/bin/timeout -k 5 "${HARD_TIMEOUT:-90}")
else
    TO=()
fi

# Translate ASCII into QEMU `sendkey` monitor commands.
emit_keys() {
    local s="$1" i c key
    for (( i=0; i<${#s}; i++ )); do
        c="${s:$i:1}"
        case "$c" in
            [a-z]) key="$c" ;;
            [A-Z]) key="shift-$(printf '%s' "$c" | tr 'A-Z' 'a-z')" ;;
            [0-9]) key="$c" ;;
            ' ')   key="spc" ;;
            '.')   key="dot" ;;
            ',')   key="comma" ;;
            '-')   key="minus" ;;
            '_')   key="shift-minus" ;;
            '/')   key="slash" ;;
            '~')   key="backspace" ;;   # harness-only token
            *)     continue ;;
        esac
        echo "sendkey $key"
    done
}

{
    sleep "$BOOT_WAIT"
    for cmd in "$@"; do
        [ -z "$cmd" ] && continue
        emit_keys "$cmd"
        echo "sendkey ret"
        sleep "$CMD_WAIT"
    done
    sleep "$END_WAIT"
    echo "pmemsave 0xB8000 4000 \"$VGA_WIN\""
    sleep 1
    echo "quit"
} | "${TO[@]}" "$QEMU" "${qargs[@]}" >"$WORK/qemu.stdout" 2>&1

if [ ! -f "$VGA_BIN" ]; then
    echo "!! BOOTCHECK FAILED: pmemsave produced no file"
    echo "--- qemu monitor output ---"
    tail -20 "$WORK/qemu.stdout"
    [ -f "$QLOG" ] && { echo "--- qemu.log tail ---"; tail -30 "$QLOG"; }
    exit 1
fi

python - "$VGA_BIN" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
rows = []
for y in range(25):
    rows.append(''.join(
        chr(d[(y*80+x)*2]) if 32 <= d[(y*80+x)*2] < 127 else ' '
        for x in range(80)).rstrip())
while rows and not rows[-1]:
    rows.pop()
print('+' + '-'*80 + '+')
for i, r in enumerate(rows):
    print('|%-80s| %2d' % (r, i))
print('+' + '-'*80 + '+')
PY

[ "${KEEPSCREEN:-0}" = "1" ] || rm -f "$VGA_BIN"
exit 0
