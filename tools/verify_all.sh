#!/bin/bash
# verify_all.sh - full acceptance run over everything built in steps 1-18.
# Slower than regress.sh (many boots); run at stage boundaries.

set -u
export PATH="/usr/bin:/bin:$PATH"
tounix() { cygpath -u "$1" 2>/dev/null || echo "$1"; }
ROOT="$(tounix 'C:/workspace/10.OS')"
WORK="$(cd "$(dirname "$0")" && pwd)"
export PATH="$(tounix 'C:/cygwin64/bin'):$(tounix 'C:/cygwin64/usr/cross/bin'):$PATH"

pass=0; fail=0
ok()   { printf '  [PASS] %s\n' "$1"; pass=$((pass+1)); }
bad()  { printf '  [FAIL] %s\n' "$1"; fail=$((fail+1)); }
sect() { printf '\n--- %s\n' "$1"; }

# want <label> <regex> <screen-text>
want() { if echo "$3" | grep -qE "$2"; then ok "$1"; else bad "$1 (no /$2/)"; fi; }
deny() { if echo "$3" | grep -qE "$2"; then bad "$1 (unexpected /$2/)"; else ok "$1"; fi; }
# literal variants - milestone names contain '+', which grep -E treats as a quantifier
wantF() { if echo "$3" | grep -qF "$2"; then ok "$1"; else bad "$1 (no '$2')"; fi; }
denyF() { if echo "$3" | grep -qF "$2"; then bad "$1 (unexpected '$2')"; else ok "$1"; fi; }

echo "=============== 10.OS full verification ==============="

sect "build"
cd "$ROOT" || exit 1
if make >"$WORK/v_build.log" 2>&1; then
    w=$(grep -c 'warning:' "$WORK/v_build.log" || true)
    sz=$(stat -c%s 02.Kernel64/Kernel64.bin)
    sec=$(grep -o 'boot loader \[[0-9]*\]' "$WORK/v_build.log" | grep -o '[0-9]*' | head -1)
    [ "${w:-0}" -le 1 ] && ok "build, warnings=${w:-0} (<=1 expected)" \
                        || bad "build warnings=${w:-0}"
    ok "Kernel64.bin=${sz}B image=${sec:-?}/1920 sectors"
else
    bad "build"; tail -20 "$WORK/v_build.log"; exit 1
fi

sect "boot integrity (no keystrokes)"
b=$("$WORK/bootcheck.sh" 2>&1)
for m in 'C Language Kernel Start' 'IA-32e Page Table Initialization' \
         'IA-32e Mode Kernel Start' 'Initializing GDT' 'Initializing IDT' \
         'Reading E820 Memory Map' 'Physical Frame Allocator' \
         'Kernel Page Tables + Direct Map' 'Slab Allocator + kmalloc' \
         'Initializing PIC Controller'; do
    wantF "init: $m" "$m" "$b"
done
denyF "no init step reported Fail" 'Fail' "$b"
deny "no exception during boot"   'Exception Occurred|KERNEL PANIC' "$b"
want "shell prompt"               'TM_OS1>' "$b"

sect "legacy features still work"
s=$(CMD_WAIT=3 "$WORK/bootcheck.sh" 'totalram' 'createtask 2 4' 'date' 'wait 50' 'rdtsc' 2>&1)
want "totalram"        'Total RAM Size' "$s"
want "createtask"      'Created' "$s"
want "date (RTC)"      'Data:.*Time:' "$s"
want "wait (PIT)"      '50\[ms\] Sleep Complete' "$s"
want "rdtsc"           'Time Stamp Counter' "$s"

sect "E820 memory map"
s=$(CMD_WAIT=3 "$WORK/bootcheck.sh" 'memmap' 2>&1)
want "usable region below 1MB"   '000000000000  00000009FC00  USABLE' "$s"
want "main usable region"        '000000100000  .*  USABLE' "$s"
want "EBDA reserved"             '00000009FC00  0000000A0000  RESERVED' "$s"
want "entry count + total"       'entries=[0-9]+ usable=[0-9]+MB' "$s"

sect "physical frame allocator"
s=$(CMD_WAIT=4 "$WORK/bootcheck.sh" 'pmemstat' 'alloctest 64 0' 'alloctest 8 10' 'pmemstat' 2>&1)
want "pmemstat reports frames"  'frames total=[0-9]+ free=[0-9]+ reserved=[0-9]+' "$s"
want "order-0 alloc"            'allocated 64 \(order 0\)' "$s"
want "order-10 alloc (4MB)"     'allocated 8 \(order 10\)' "$s"
deny "no misaligned block"      'MISALIGNED' "$s"
deny "no corrupted pattern"     'PATTERN CORRUPT' "$s"
deny "no frame leak"            'LEAK' "$s"
# the buddy allocator pops from a free list, so the first address returned is
# no longer the lowest frame - check the frame's own state instead
s2=$(CMD_WAIT=3 "$WORK/bootcheck.sh" 'cls' 'frameinfo 100000' 'frameinfo 200000' 2>&1)
want "reclaimed Kernel32 tables" 'frame 000000100000: allocatable' "$s2"
want "kernel image still reserved" 'frame 000000200000: RESERVED' "$s2"

sect "paging: 4KB split, W^X flags, direct map"
# walk the HIGH alias - that is where the kernel executes and where the 4KB
# split lives. There is no identity alias any more - the low half is empty.
s=$(CMD_WAIT=3 "$WORK/bootcheck.sh" 'cls' 'pgtest' 'pgwalk FFFFFFFF80202000' 2>&1)
# pgtest locates the sections from the linker symbols, so this does not go
# stale when the kernel grows and the boundaries move
want ".text  RO+X"                  'text at [0-9A-F]+: RO\+X' "$s"
want ".rodata RO+NX"                'rodata at [0-9A-F]+: RO\+NX' "$s"
want ".data  RW+NX"                 'data at [0-9A-F]+: RW\+NX' "$s"
deny "no section left as 2MB page"  'NOT 4KB' "$s"
want "4KB pages over kernel image"  '4KB page' "$s"
want "kernel executes from high alias" 'VA FFFFFFFF80202000.*|-> PA 000000202000' "$s"
want "CR0.WP on"                    'WP=on' "$s"
want "NX supported"                 'NX=supported' "$s"
want "direct map alias->direct"     'alias->direct OK' "$s"
want "low half has no identity map" 'low half clear' "$s"
want "direct map direct->alias"     'direct->alias OK' "$s"
want "direct map VA"                'direct map VA = FFFF8000' "$s"

sect "slab + kmalloc"
s=$(CMD_WAIT=4 "$WORK/bootcheck.sh" 'kmalloctest 64' 'slabinfo' 'kmalloctest 64' 2>&1)
want "kmalloc pattern intact"  'pattern OK' "$s"
deny "no overlapping blocks"   'OVERLAP' "$s"
want "12 kmalloc caches"       'kmalloc-16384' "$s"
# ERE has no backreferences; compare the two counts in shell instead
r1=$(echo "$s" | grep -oE 'frames [0-9]+ -> [0-9]+' | sed -n '2p')
if [ -n "$r1" ] && [ "$(echo "$r1" | awk '{print $2}')" = "$(echo "$r1" | awk '{print $4}')" ]; then
    ok "slab reuse (2nd run consumes no frames: $r1)"
else
    bad "slab reuse (2nd run: ${r1:-missing})"
fi

sect "exception paths"
for t in div0:'Vector : 0 ' ud:'Vector : 6 ' gp:'Vector : 13' pf:'Vector : 14' \
         wtext:'err=0x0003|ErrCode: 0x0000000000000003' \
         xdata:'err=0x0011|ErrCode: 0x0000000000000011'; do
    cmd="${t%%:*}"; pat="${t#*:}"
    SERIAL="$WORK/v_$cmd.log" CMD_WAIT=4 "$WORK/bootcheck.sh" "crash $cmd" >/dev/null 2>&1
    l=$(cat "$WORK/v_$cmd.log" 2>/dev/null)
    want "crash $cmd" "$pat" "$l"
    want "crash $cmd halts cleanly" 'System Halted' "$l"
done

sect "scales across RAM sizes"
for m in 32 64 256 1024; do
    s=$(MEM=$m CMD_WAIT=3 "$WORK/bootcheck.sh" 'pmemstat' 2>&1)
    if [ "$m" = "32" ]; then
        want "-m 32 correctly rejected" 'Minimum Memory Size.*Fail' "$s"
    else
        want "-m $m boots + allocator up" 'frames total=[0-9]+' "$s"
    fi
done

sect "preemption soak on the new tables (20 tasks, 1ms, 40s)"
s=$(CMD_WAIT=3 END_WAIT=40 HARD_TIMEOUT=140 "$WORK/bootcheck.sh" \
        'settimer 1 1' 'createtask 2 20' 2>&1)
want "20 tasks created"      'Task2 20 Created' "$s"
deny "no exception in soak"  'Exception Occurred|KERNEL PANIC' "$s"

echo
sect "syscall: int 0x80 gate and dispatcher"
s=$(CMD_WAIT=4 "$WORK/bootcheck.sh" 'cls' 'syscalltest' 2>&1)
want "sys_write reaches the console" 'hello from int 0x80' "$s"
want "sys_write returns the length"  'sys_write  -> 20 \(len 20\)' "$s"
# ERE has no backreferences, so the kernel compares the two itself
want "sys_getpid matches scheduler"  'sys_getpid -> [0-9A-F]+  running=[0-9A-F]+  MATCH' "$s"
want "sys_uptime returns ticks"      'sys_uptime -> [0-9A-F]+ ticks' "$s"
want "unknown call is -ENOSYS"       'bad call   -> -38' "$s"
want "bad fd is -EBADF"              'bad fd     -> -9' "$s"
want "every call dispatched"         'dispatched 5 syscalls' "$s"

sect "user mappings: low half and US propagation"
s=$(CMD_WAIT=3 "$WORK/bootcheck.sh" 'cls' 'maptest 400000' 'maptest 500000 user' 2>&1)
# the low half is what the identity map used to occupy - user space goes here
want "4KB map below RAM top"     'VA 0000000000400000: mapped, readback OK' "$s"
want "kernel map stays supervisor" 'US levels 0/4 \(want 0\)' "$s"
# x86-64 ANDs U/S across all four levels, so a leaf-only US is unreachable
want "US reaches every level"    'US levels 4/4 \(want 4\)' "$s"

sect "process address space: mm_struct and VMAs"
s=$(CMD_WAIT=4 "$WORK/bootcheck.sh" 'cls' 'mmtest' 2>&1)
want "mm allocates a PML4"       'mm created pml4=[0-9A-F]+ mms=1' "$s"
want "VMA insert/find/overlap"   'vma find hit=OK miss=OK overlap=rejected count=2' "$s"
want "user page is US at all 4"  'US levels 4/4' "$s"
# surviving mov cr3 at all proves the kernel half is shared into the new PML4
want "write through a switched CR3" 'user write via CR3 switch: OK' "$s"
want "address space fully freed" 'mm destroyed mms=0 .* OK' "$s"
deny "no frames leaked"          'LEAK' "$s"

sect "per-task address space: CR3 follows the context switch"
s=$(CMD_WAIT=5 "$WORK/bootcheck.sh" 'cls' 'cr3test' 2>&1)
want "task runs on its own CR3"  'task cr3=[0-9A-F]+ want=[0-9A-F]+ OK' "$s"
# the VA is mapped only in that mm, so reading it proves the switch happened
want "task reads its private VA" 'task read private VA: OK' "$s"
want "kernel thread borrows CR3" 'shell borrowed mm cr3: yes' "$s"
want "destroy restores kernel CR3" 'after destroy cr3 is kernel: OK' "$s"
deny "no fault during the switch" 'KERNEL PANIC|Exception Occurred' "$s"

sect "ring3: user mode and a syscall from it"
s=$(CMD_WAIT=5 "$WORK/bootcheck.sh" 'cls' 'usertest' 2>&1)
want "user image mapped"        'stub [0-9A-F]+ bytes at 00400000  stack pages 4  map OK' "$s"
# printed by the ring3 stub itself, so the whole int 0x80 path ran from user mode
want "sys_write from ring3"     'ring3 syscall ok' "$s"
# the CPU pushed this CS when it preempted the task - output alone proves nothing
want "task really was at CPL 3" 'saved CS=23 CPL=3 ring3' "$s"
want "address space reclaimed"  'free before=[0-9A-F]+ after=[0-9A-F]+ OK' "$s"
deny "no fault in user mode"    'KERNEL PANIC|Exception Occurred' "$s"

sect "page fault: a bad user program dies alone"
s=$(CMD_WAIT=6 "$WORK/bootcheck.sh" 'cls' 'usertest bad' 'date' 2>&1)
want "user reaches ring3 first"  'ring3 about to touch kernel' "$s"
# present+read from user mode means the U/S check rejected it, not a missing page
want "SEGV on a kernel address" 'SEGV task [0-9A-F]+ at FFFFFFFF80200000 \(read,prot\) kernel address' "$s"
want "only that task died"      'task died \(SEGV\)  killed so far=1' "$s"
want "shell survived the fault" 'ring3 task reaped, shell alive' "$s"
# the shell taking a command afterwards is the real proof the kernel is intact
want "shell still takes commands" 'Data: [0-9]+/[0-9]+/[0-9]+' "$s"
deny "kernel did not panic"     'KERNEL PANIC' "$s"
want "no frames leaked"         'free before=[0-9A-F]+ after=[0-9A-F]+ OK' "$s"

echo "=============== $pass passed, $fail failed ==============="
exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
