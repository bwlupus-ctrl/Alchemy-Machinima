"""D56: exact deployed bridge/ABI and absence of ReShade key consumers.

Only the explicitly listed C++/header/build/doc paths are opened; no .fx/.fxh.
Behavioral refusal mutations are exercised by WP2, snapshot and UI checks.
"""
from pathlib import Path
import json
import re
import subprocess
from relight_area_policy_check import method

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
for name in ('indra/newview/llreshadebridge.cpp', 'indra/newview/llreshadebridge.h',
             'indra/newview/llreshadebridgeabi.h', 'reshade-addon/CMakeLists.txt',
             'reshade-addon/README.md', 'reshade-addon/src/sl_reshade_bridge.cpp'):
    expected = subprocess.check_output(['git', 'show', '27d77b5c272:' + name], cwd=ROOT)
    assert (ROOT/name).read_bytes() == expected, name
for name in ('cmake/SLAddonSourceHash.cmake', 'cmake/SLAddonSourceHash.py',
             'scripts/deploy_addon.ps1', 'scripts/code_tools/actor_key_wp3_check.py',
             'scripts/code_tools/actor_key_wp3_whitelist_check.py',
             'scripts/code_tools/actor_key_wp3_68_check.py',
             'reshade-addon/src/sl_key_guard.hpp', 'reshade-addon/src/sl_key_bracket.hpp',
             'reshade-addon/src/sl_key_guard_events.inl', 'reshade-addon/src/sl_key_whitelist.hpp',
             'reshade-addon/src/sl_module_watch.hpp'):
    assert not (ROOT/name).exists(), name

def independent(source):
    assert not re.search(r'reshade|keyGuardAllows|retryKeyGuard|SLKey::', source, re.I)

for name in ('alactorchromakey.cpp', 'alactorchromakey.h', 'alpanelactorkey.cpp', 'alpanelactorkey.h'):
    independent((NV/name).read_text(encoding='utf-8'))
for name, signatures in {
    'pipeline.cpp': ('bool LLPipeline::preflightActorKey(',),
    'llviewerwindow.cpp': ('bool LLViewerWindow::rawSnapshot(',),
    'alactorchromakey.cpp': ('bool ALActorChromaKey::snapshotRequestAllowed(',
                           'void ALActorChromaKey::snapshotRefused('),
}.items():
    for signature in signatures:
        production = method(NV/name, signature)
        independent(production)
        try:
            independent(production + '\nif (reshade_present) return false;')
        except AssertionError:
            pass
        else:
            raise AssertionError('ReShade refusal mutation escaped: ' + signature)
data = (ROOT/'scripts/code_tools/director_kit/director_mockup_data.json').read_bytes()
assert data.endswith(b'\n') and data.count(b'\n') == 1
assert json.dumps(json.loads(data), ensure_ascii=False, separators=(',', ':')).encode() + b'\n' == data
print('PASS D56: bridge/add-on byte-identical to pre-WP3 (ABI 1.2); guard helpers removed; key/K0/snapshot/latch/UI independent of ReShade; refusal mutations rejected; compact kit JSON with trailing newline')
