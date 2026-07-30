#!/bin/bash
# regress.sh - the "기본 회귀 검사" from the plan, run after EVERY step.
#
#   1. make succeeds, Kernel64.bin is a sane size, no new warnings
#   2. boot #1 (no keystrokes, so nothing scrolls): every init milestone
#      present on screen, none reporting Fail, no exception
#   3. boot #2 (types shell commands): totalram / createtask / date behave
#
# Extra shell commands may be appended as arguments; they run in boot #2.
# Exit 0 = PASS.

set -u
export PATH="/usr/bin:/bin:$PATH"
tounix() { cygpath -u "$1" 2>/dev/null || echo "$1"; }

ROOT="$(tounix 'C:/workspace/10.OS')"
WORK="$(cd "$(dirname "$0")" && pwd)"
export PATH="$(tounix 'C:/cygwin64/bin'):$(tounix 'C:/cygwin64/usr/cross/bin'):$PATH"

fail=0
say() { printf '  %-40s %s\n' "$1" "$2"; }

echo "=============== 10.OS regression ==============="

# --- 1. build -------------------------------------------------------------
cd "$ROOT" || exit 1
if make >"$WORK/build.log" 2>&1; then
    sz=$(stat -c%s 02.Kernel64/Kernel64.bin)
    sect=$(grep -o 'Total sector count except boot loader \[[0-9]*\]' "$WORK/build.log" \
           | grep -o '[0-9]*$')
    if [ "$sz" -gt 200000 ]; then
        say "build" "FAIL (Kernel64.bin ${sz}B - implausible)"; fail=1
    else
        say "build" "PASS (Kernel64.bin ${sz}B, image ${sect:-?} sectors / 1920 max)"
    fi
else
    say "build" "FAIL"; tail -30 "$WORK/build.log"; exit 1
fi
nwarn=$(grep -c 'warning:' "$WORK/build.log" 2>/dev/null || true)
# 1 expected: "LOAD segment with RWX permissions" (inherent to a flat kernel)
if [ "${nwarn:-0}" -gt 1 ]; then
    say "compiler warnings" "${nwarn} (expected 1, see build.log)"; fail=1
fi

# --- 2. boot with no keystrokes: full init log stays on screen ------------
boot=$("$WORK/bootcheck.sh" 2>&1)
if [ $? -ne 0 ]; then
    say "boot" "FAIL (harness error)"; echo "$boot"; exit 1
fi

miss=0
for m in \
    'C Language Kernel Start' \
    'Check Minimum Memory Size' \
    'IA-32e Kernel Area Initialization' \
    'IA-32e Page Table Initialization' \
    'Check CPU support 64Bit Mode' \
    'Copy Kernel Code to Memory' \
    'IA-32e Mode Kernel Start' \
    'Initialize Console' \
    'Initializing GDT' \
    'Initializing TSS Segment' \
    'Initializing IDT' \
    'Reading E820 Memory Map' \
    'Check System RAM Size' \
    'Physical Frame Allocator' \
    'CTCB Pool And Scheduler Initialize' \
    'Initializing Keyboard Interface' \
    'Initializing PIC Controller'
do
    if ! echo "$boot" | grep -qF "$m"; then
        say "init missing: $m" "FAIL"; miss=1; fail=1
    fi
done
nok=$(echo "$boot" | grep -c 'OK' || true)
[ "$miss" -eq 0 ] && say "init milestones" "PASS ($nok lines report OK)"

if echo "$boot" | grep -qF 'Fail'; then
    say "no init step failed" "FAIL"; echo "$boot" | grep -F 'Fail'; fail=1
else
    say "no init step failed" "PASS"
fi
if echo "$boot" | grep -qiE 'Exception Occurred|KERNEL PANIC|EXCEPTION DUMP'; then
    say "no exception during boot" "FAIL"; fail=1
else
    say "no exception during boot" "PASS"
fi
echo "$boot" | grep -qF 'TM_OS1>' && say "shell prompt" "PASS" \
    || { say "shell prompt" "FAIL"; fail=1; }

# --- 3. boot with shell commands ------------------------------------------
scr=$(CMD_WAIT=3 "$WORK/bootcheck.sh" "totalram" "createtask 2 4" "date" "$@" 2>&1)
check() {
    if echo "$scr" | grep -qE "$2"; then say "$1" "PASS"
    else say "$1" "FAIL  (no match for /$2/)"; fail=1; fi
}
check "totalram"       'Total RAM Size'
check "createtask 2 4" 'Created'
check "date"           'Data:.*Time:'

echo
if [ "$fail" -ne 0 ]; then
    echo "----- boot screen -----"; echo "$boot"
    echo "----- command screen -----"; echo "$scr"
    echo "=============== REGRESSION FAIL ==============="
else
    echo "=============== REGRESSION PASS ==============="
fi
exit "$fail"
