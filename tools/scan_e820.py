import struct
import sys

TYPES = {1: 'USABLE', 2: 'RESERVED', 3: 'ACPI_RECLAIM',
         4: 'ACPI_NVS', 5: 'BAD'}

d = open(sys.argv[1], 'rb').read()
magic, = struct.unpack_from('<I', d, 0)
count, = struct.unpack_from('<H', d, 4)

print('magic  : 0x%08X %s' % (magic, '(BTIF ok)' if magic == 0x46495442 else '(BAD)'))
print('entries: %d' % count)
if magic != 0x46495442:
    sys.exit(1)

total_usable = 0
print('  %-18s %-18s %-14s %s' % ('base', 'length', 'end', 'type'))
for i in range(count):
    base, length, typ, ext = struct.unpack_from('<QQII', d, 8 + i * 24)
    name = TYPES.get(typ, 'TYPE%d' % typ)
    print('  0x%016X 0x%016X 0x%012X %s' % (base, length, base + length, name))
    if typ == 1:
        total_usable += length
print('usable total: %d bytes = %d MB' % (total_usable, total_usable // (1024 * 1024)))
