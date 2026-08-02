import sys

BASE = 0x142000
IDT = 0x1420B0                 # IDT_START_ADDR
LEGAL_END = IDT + 100 * 16     # 0x1426F0, IDT_ENTRY_SIZE gates

d = open(sys.argv[1], 'rb').read()


def is_gate(e):
    # 16-byte gate written by kSetIDTEntry: selector 0x0008 at +2,
    # IST=1 at +4, type/flags 0x8E at +5, reserved dword 0 at +12
    return (len(e) == 16 and e[2:4] == b'\x08\x00'
            and e[4] == 0x01 and e[5] == 0x8E and e[12:16] == b'\x00' * 4)


last = None
count = 0
for off in range(IDT - BASE, len(d) - 16 + 1, 16):
    if is_gate(d[off:off + 16]):
        last = BASE + off
        count += 1

print('IDT_START_ADDR    : 0x%X' % IDT)
print('legal end (100)   : 0x%X' % LEGAL_END)
if last is None:
    print('no gates found')
    sys.exit(1)
print('gates found       : %d' % count)
print('last gate at      : 0x%X (ends 0x%X)' % (last, last + 16))
over = (last + 16) - LEGAL_END
if over > 0:
    print('>>> OVERRUN       : %d bytes past the legal end' % over)
else:
    print('>>> clean         : nothing written past the legal end')
