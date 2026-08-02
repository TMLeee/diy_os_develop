#!/bin/bash
# dumpmem.sh <hexaddr> <bytes> [outfile]
# Boots Disk.img headless and pmemsave's a physical range out of the guest.
set -u
export PATH="/usr/bin:/bin:$PATH"
tounix() { cygpath -u "$1" 2>/dev/null || echo "$1"; }
towin()  { cygpath -m "$1" 2>/dev/null || echo "$1"; }

ROOT="$(tounix 'C:/workspace/10.OS')"
QEMU="$(tounix 'C:/Program Files/qemu/qemu-system-x86_64.exe')"
WORK="$(cd "$(dirname "$0")" && pwd)"

ADDR="$1"; SIZE="$2"; OUT="${3:-$WORK/mem.bin}"
rm -f "$OUT"

{
    sleep "${BOOT_WAIT:-5}"
    echo "pmemsave $ADDR $SIZE \"$(towin "$OUT")\""
    sleep 1
    echo "quit"
} | /usr/bin/timeout -k 5 60 "$QEMU" -display none -monitor stdio \
        -m "${MEM:-64}" -fda "$(towin "$ROOT/Disk.img")" -boot a -M pc -no-reboot \
        >"$WORK/qemu.stdout" 2>&1

[ -f "$OUT" ] || { echo "!! pmemsave failed"; tail -5 "$WORK/qemu.stdout"; exit 1; }
echo "dumped $SIZE bytes from $ADDR -> $OUT"
