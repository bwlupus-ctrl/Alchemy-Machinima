"""Canonical ABI/add-on manifest; never reads effect directories."""
from pathlib import Path
import hashlib
import sys

def manifest(root):
    paths = [p for p in (root/'reshade-addon/src').rglob('*') if p.is_file()
             and p.suffix.lower() in ('.cpp','.hpp','.h','.inl') and p.name != 'sl_bridge_build_info.h']
    paths += [root/'reshade-addon/CMakeLists.txt',root/'indra/newview/llreshadebridgeabi.h']
    return sorted(paths,key=lambda p:p.relative_to(root).as_posix().encode('utf-8'))

def digest(root):
    h=hashlib.sha256()
    for p in manifest(root):
        content=p.read_bytes().removeprefix(b'\xef\xbb\xbf').replace(b'\r\n',b'\n').replace(b'\r',b'\n')
        h.update(p.relative_to(root).as_posix().encode('utf-8')+b'\n'+content+b'\0')
    return h.hexdigest()

if __name__=='__main__':
    import subprocess
    from datetime import datetime,timezone
    root=Path(sys.argv[1]).resolve();value=digest(root)
    commit=subprocess.run(['git','-C',str(root),'rev-parse','--short=12','HEAD'],capture_output=True,text=True).stdout.strip()
    header=('// Generated at configure.\n#pragma once\n'
      +'#define SL_EXPECTED_ADDON_SOURCE_SHA256 "'+value+'"\n'
      +'#define SL_ADDON_SOURCE_SHA256 {'+','.join('0x'+value[i:i+2] for i in range(0,64,2))+'}\n'
      +'#define SL_ADDON_COMMIT "'+commit+'"\n'
      +'#define SL_ADDON_BUILD_STAMP "'+datetime.now(timezone.utc).strftime('%Y-%m-%d %H:%M:%S UTC')+'"\n')
    Path(sys.argv[2]).write_text(header,encoding='utf-8')
    print(value)
    for p in manifest(root):print(p.relative_to(root).as_posix())
