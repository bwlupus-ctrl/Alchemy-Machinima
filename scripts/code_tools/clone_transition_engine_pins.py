"""Exact additive allowance for the clone-transition ENGINE layer (workstream E, Phase 1).

Same discipline as clone_transition_pins.py (the render layer): older gates rebuild the "feature chain" of
the Studio / action / menu / panel files from the CURRENT source by peeling reviewed layers off the top.
reverse_engine_layer() peels exactly the engine hunks recorded in clone_transition_engine_hunks.json (pinned
by digest) and requires the result to equal base a49168443e7. An unreviewed edit in the same files still fails
the older gates.

HOOKING (needs the render workstream's module, so it is a REQUEST in REPORT_ENGINE.md): add to
clone_transition_pins.reverse_clone_transition, before its own work,
    from clone_transition_engine_pins import reverse_engine_layer
    source = reverse_engine_layer(source, path)

Maintenance: after an intentional edit of a covered file run
    python scripts/code_tools/clone_transition_engine_pins.py --regen
and paste the printed digest into MANIFEST_SHA256 below.
"""
from pathlib import Path
import hashlib
import json
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from clone_transition_pins import ROOT, BASE, _git_base, _hunks  # noqa: E402  (shared hunk maker)

MANIFEST = Path(__file__).with_name('clone_transition_engine_hunks.json')
MANIFEST_SHA256 = 'fc1778aa00622b8fde2419b0470330845f3b07eb31a48788e9caf7097ba28928'
COVERED = (
    'indra/newview/alghoststudio.cpp', 'indra/newview/alghoststudio.h',
    'indra/newview/alghostcloneactions.cpp', 'indra/newview/alghostcloneactions.h',
    'indra/newview/alghostclonemenu.cpp',
    'indra/newview/alpanelghoststudio.cpp', 'indra/newview/alpanelghoststudio.h',
    'indra/newview/llappviewer.cpp', 'indra/newview/aldirectoralloff.cpp',
)


def regen():
    files = {p: _hunks(_git_base(p), (ROOT / p).read_bytes().decode('utf-8').replace('\r\n', '\n')) for p in COVERED}
    data = {'base': BASE, 'description': 'clone-transition engine layer (workstream E)', 'files': files}
    raw = (json.dumps(data, indent=1, sort_keys=True) + '\n').encode('utf-8')
    MANIFEST.write_bytes(raw)
    print('manifest', MANIFEST.name, len(files), 'files, sha256', hashlib.sha256(raw).hexdigest())


def _load():
    raw = MANIFEST.read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(raw).hexdigest() == MANIFEST_SHA256, ('clone transition engine allowance pin', hashlib.sha256(raw).hexdigest())
    data = json.loads(raw)
    assert data['base'] == BASE
    return data['files']


def reverse_engine_layer(source, path):
    """Peel the transition engine layer off `source` (a covered file's current text). Paths not covered, and
    sources that already equal the base, pass through untouched."""
    if not MANIFEST.exists() or MANIFEST_SHA256.strip('0') == '':
        return source
    files = _load()
    key = path if path in files else next((p for p in files if p.endswith('/' + path)), None)
    if key is None:
        return source
    original = _git_base(key)
    if source == original:
        return source
    expected = original
    for hunk in files[key]:
        assert expected.count(hunk['before']) == 1, ('clone transition engine baseline hunk', key)
        expected = expected.replace(hunk['before'], hunk['after'], 1)
    assert source == expected, ('clone transition engine layer: full feature source', key)
    for hunk in reversed(files[key]):
        assert source.count(hunk['after']) == 1, ('clone transition engine exact hunk', key)
        source = source.replace(hunk['after'], hunk['before'], 1)
    assert source == original, ('clone transition engine reversal must reproduce its exact baseline', key)
    return source


if __name__ == '__main__':
    if '--regen' in sys.argv:
        regen()
    else:
        files = _load()
        for p in files:
            cur = (ROOT / p).read_bytes().decode('utf-8').replace('\r\n', '\n')
            assert reverse_engine_layer(cur, p) == _git_base(p), p
        print('clone_transition_engine_pins: PASS', len(files), 'files reverse to base')
