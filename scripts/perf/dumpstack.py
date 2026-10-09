"""Usage: dumpstack.py <dump> <exe>. Minimal minidump crash-stack reader: exception address + heuristic stack scan,
symbolized with the system dbghelp against the module's PDB."""
import ctypes, ctypes.wintypes as wt, struct, sys, os

dump, exe = sys.argv[1], sys.argv[2]
data = open(dump, 'rb').read()
sig, ver, nstreams, dir_rva = struct.unpack_from('<IIII', data, 0)
streams = {}
for i in range(nstreams):
    t, size, rva = struct.unpack_from('<III', data, dir_rva + 12 * i)
    streams[t] = (size, rva)

# modules
mods = []
size, rva = streams[4]
(n,) = struct.unpack_from('<I', data, rva)
for i in range(n):
    off = rva + 4 + 108 * i
    base, sz = struct.unpack_from('<QI', data, off)
    name_rva = struct.unpack_from('<I', data, off + 20)[0]
    ln = struct.unpack_from('<I', data, name_rva)[0]
    name = data[name_rva + 4:name_rva + 4 + ln].decode('utf-16le')
    mods.append((base, sz, name))

def mod_of(a):
    for b, s, nm in mods:
        if b <= a < b + s:
            return b, s, nm
    return None

# exception
size, rva = streams[6]
tid = struct.unpack_from('<I', data, rva)[0]
code, flags = struct.unpack_from('<II', data, rva + 8)
addr = struct.unpack_from('<Q', data, rva + 8 + 16)[0]
nparams = struct.unpack_from('<I', data, rva + 8 + 24)[0]
info = struct.unpack_from('<15Q', data, rva + 8 + 32)
ctx_size, ctx_rva = struct.unpack_from('<II', data, rva + 8 + 32 + 120)
rsp = struct.unpack_from('<Q', data, ctx_rva + 0x98)[0]
rip = struct.unpack_from('<Q', data, ctx_rva + 0xF8)[0]
print(f'thread {tid} code {code:#x} addr {addr:#x} params {[hex(x) for x in info[:nparams]]}')

# memory ranges
ranges = []
if 9 in streams:
    size, rva = streams[9]
    cnt, base_rva = struct.unpack_from('<QQ', data, rva)
    cur = base_rva
    for i in range(cnt):
        start, sz = struct.unpack_from('<QQ', data, rva + 16 + 16 * i)
        ranges.append((start, sz, cur)); cur += sz
if 5 in streams:
    size, rva = streams[5]
    (cnt,) = struct.unpack_from('<I', data, rva)
    for i in range(cnt):
        start, dsz, drva = struct.unpack_from('<QII', data, rva + 4 + 16 * i)
        ranges.append((start, dsz, drva))

def read(a, n):
    for start, sz, r in ranges:
        if start <= a and a + n <= start + sz:
            return data[r + (a - start): r + (a - start) + n]
    return None

# dbghelp
dbg = ctypes.WinDLL(r'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Platform\DiagnosticsHub\amd64\dbghelp.dll')
h = wt.HANDLE(0x1234)
dbg.SymSetOptions(0x2 | 0x10 | 0x4)  # UNDNAME | LOAD_LINES | DEFERRED_LOADS
dbg.SymInitializeW.argtypes = [wt.HANDLE, wt.LPCWSTR, wt.BOOL]
dbg.SymInitializeW(h, os.path.dirname(exe), False)
dbg.SymLoadModuleExW.argtypes = [wt.HANDLE, wt.HANDLE, wt.LPCWSTR, wt.LPCWSTR, ctypes.c_uint64, wt.DWORD, ctypes.c_void_p, wt.DWORD]
dbg.SymLoadModuleExW.restype = ctypes.c_uint64
for b, s, nm in mods:
    if os.path.basename(nm).lower() == os.path.basename(exe).lower():
        r = dbg.SymLoadModuleExW(h, None, exe, None, b, s, None, 0)
        print('loaded', nm, hex(b), 'ret', hex(r))

class SYMBOL_INFOW(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wt.ULONG), ('TypeIndex', wt.ULONG), ('Reserved', ctypes.c_uint64 * 2),
                ('Index', wt.ULONG), ('Size', wt.ULONG), ('ModBase', ctypes.c_uint64), ('Flags', wt.ULONG),
                ('Value', ctypes.c_uint64), ('Address', ctypes.c_uint64), ('Register', wt.ULONG),
                ('Scope', wt.ULONG), ('Tag', wt.ULONG), ('NameLen', wt.ULONG), ('MaxNameLen', wt.ULONG),
                ('Name', wt.WCHAR * 1024)]
class LINEW(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wt.DWORD), ('Key', ctypes.c_void_p), ('LineNumber', wt.DWORD),
                ('FileName', wt.LPWSTR), ('Address', ctypes.c_uint64)]

def sym(a):
    m = mod_of(a)
    mname = os.path.basename(m[2]) if m else '?'
    si = SYMBOL_INFOW(); si.SizeOfStruct = 88; si.MaxNameLen = 1024
    disp = ctypes.c_uint64(0)
    s = f'{mname}+{a - m[0]:#x}' if m else hex(a)
    if dbg.SymFromAddrW(h, ctypes.c_uint64(a), ctypes.byref(disp), ctypes.byref(si)):
        s = f'{mname}!{si.Name}+{disp.value:#x}'
        ln = LINEW(); ln.SizeOfStruct = ctypes.sizeof(LINEW); d32 = wt.DWORD(0)
        if dbg.SymGetLineFromAddrW64(h, ctypes.c_uint64(a), ctypes.byref(d32), ctypes.byref(ln)):
            s += f'  [{os.path.basename(ln.FileName)}:{ln.LineNumber}]'
    return s

print('RIP', sym(rip))
print('--- heuristic stack (return addresses into the viewer exe) ---')
exe_mod = [m for m in mods if os.path.basename(m[2]).lower() == os.path.basename(exe).lower()][0]
shown = 0
for off in range(0, 0x6000, 8):
    raw = read(rsp + off, 8)
    if raw is None:
        break
    v = struct.unpack('<Q', raw)[0]
    if exe_mod[0] <= v < exe_mod[0] + exe_mod[1]:
        print(f'  rsp+{off:#06x} {sym(v)}')
        shown += 1
        if shown >= 45:
            break



