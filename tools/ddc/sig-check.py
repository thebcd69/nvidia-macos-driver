#!/usr/bin/env python3
# sig-check.py <NVRM Mach-O> [nvrm-ddc.inc] -- check the NVRM entry-point signatures in nvrm-ddc.inc against an NVRM
# binary (the one in /Library/Extensions/NVRM.kext/Contents/MacOS/NVRM or a release tarball). thebcd69 fork.
# Prints, per signature, the symbol it should sit on (from the binary's symbol table) and whether the bytes match.
# Run with python3 -I: the binary is data, nothing in it is executed.
import re, struct, sys, os
binp = sys.argv[1]
inc = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../kexts/NVRMFB/fb/nvrm-ddc.inc')
data = open(binp, 'rb').read()
if struct.unpack_from('<I', data, 0)[0] != 0xfeedfacf: sys.exit('STOP: not a 64-bit Mach-O')
ncmds = struct.unpack_from('<I', data, 16)[0]; off = 32; segs = []; symtab = None
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', data, off)
    if cmd == 0x19:   # LC_SEGMENT_64
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', data, off + 24)
        segs.append((vmaddr, vmsize, fileoff, filesize))
    elif cmd == 0x2:  # LC_SYMTAB
        symtab = struct.unpack_from('<IIII', data, off + 8)
    off += size
def at(va, n):
    for vmaddr, vmsize, fileoff, filesize in segs:
        if vmaddr <= va and va + n <= vmaddr + filesize: return data[fileoff + va - vmaddr: fileoff + va - vmaddr + n]
    return None
syms = {}
if symtab:
    symoff, nsyms, stroff, strsize = symtab
    for i in range(nsyms):
        strx, ntype, nsect, ndesc, nvalue = struct.unpack_from('<IBBHQ', data, symoff + 16 * i)
        if ntype & 0x0e == 0x0e and nvalue:
            name = data[stroff + strx: data.index(b'\0', stroff + strx)].decode('latin-1')
            syms.setdefault(nvalue, name)
src = open(inc).read()
base = syms_by = {v: k for k, v in syms.items()}.get('_nv_xnu_phys_for_kernel_va')
if base is None: sys.exit('STOP: no _nv_xnu_phys_for_kernel_va in the symbol table (stripped binary?)')
ok = True
for m in re.finditer(r'static const Sig (k\w+)\s*=\s*\{\s*([^,]+),\s*(\d+),\s*\{([^}]*)\}\s*,\s*([^}]+?)\s*\};', src, re.S):
    name, delta, n, bs, mask = m.group(1), m.group(2), int(m.group(3)), m.group(4), m.group(5).strip()
    d = eval(delta.replace('0x', '0x'), {})
    b = [int(x, 0) for x in bs.replace('\n', ' ').split(',') if x.strip()]
    mm = re.match(r'NVDDC_M\((\d+),\s*(\d+)\)', mask)
    msk = ((1 << int(mm.group(2))) - 1) << int(mm.group(1)) if mm else int(mask, 0)
    got = at(base + d, n)
    good = got is not None and all((msk >> i) & 1 or got[i] == b[i] for i in range(n))
    ok &= good
    print(f"{name:9s} va 0x{base + d:x} sym {syms.get(base + d, '?'):32s} {'MATCH' if good else 'MISMATCH'}"
          + ('' if good or got is None else '  have ' + got.hex(' ')))
print('all signatures match' if ok else 'SIGNATURE MISMATCH: DDC would stay off with this NVRM')
sys.exit(0 if ok else 1)
