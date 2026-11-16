"""Exact additive allowance for the clone-transition RENDER layer (workstream R, Phase 1).

Every older gate that reconstructs "the feature chain" of a renderer file (llactormover.cpp/.h,
lldrawpool.cpp/.h, pipeline.cpp, lldirectorcast.cpp/.h, the actor-ghost and native avatar shaders) starts
from the CURRENT source and peels reviewed layers off the top until it reaches the immutable base. This
module is the newest layer: reverse_clone_transition() peels exactly the transition hunks recorded in
clone_transition_render_hunks.json (pinned by digest) and requires the result to equal base a49168443e7
plus whatever older layers sit below. Unreviewed edits in the same files therefore still fail the older
gates.

Maintenance: after an intentional edit of a covered file run
    python scripts/code_tools/clone_transition_pins.py --regen
and paste the printed digest into MANIFEST_SHA256 below (the same discipline as every other pin).
"""
from pathlib import Path
import difflib
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
BASE = 'a49168443e7'
MANIFEST = Path(__file__).with_name('clone_transition_render_hunks.json')
MANIFEST_SHA256 = '424707e0892fec0adec7590ddd5d131c9561b1cab95a34943d1ff77a386ddddb'
# Files this layer may touch. Everything else is "unchanged by this layer".
COVERED_PREFIXES = (
    'indra/newview/app_settings/shaders/',
    'indra/newview/llactormover.cpp', 'indra/newview/llactormover.h',
    'indra/newview/lldrawpool.cpp', 'indra/newview/lldrawpool.h',
    'indra/newview/pipeline.cpp', 'indra/newview/lldirectorcast.cpp', 'indra/newview/lldirectorcast.h',
)
CONTEXT = 3


def _git_base(path):
    return subprocess.check_output(['git', 'show', BASE + ':' + path], cwd=ROOT).decode('utf-8').replace('\r\n', '\n')


def _changed_paths():
    out = subprocess.check_output(['git', 'diff', '--name-only', BASE, '--'] + list(COVERED_PREFIXES), cwd=ROOT).decode()
    return sorted(p for p in out.split('\n') if p)


def _hunks(base, new):
    a, b = base.split('\n'), new.split('\n')
    ops = [op for op in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if op[0] != 'equal']
    groups = []
    for op in ops:
        if groups and op[1] - groups[-1][-1][2] <= 2 * CONTEXT + 1:
            groups[-1].append(op)
        else:
            groups.append([op])
    hunks = []
    for group in groups:
        i1, i2, j1, j2 = group[0][1], group[-1][2], group[0][3], group[-1][4]
        ctx = CONTEXT
        while True:
            lo_a, hi_a = max(0, i1 - ctx), min(len(a), i2 + ctx)
            lo_b, hi_b = max(0, j1 - ctx), min(len(b), j2 + ctx)
            before = '\n'.join(a[lo_a:hi_a])
            after = '\n'.join(b[lo_b:hi_b])
            if (base.count(before) == 1 and new.count(after) == 1) or ctx > 60:
                break
            ctx += 3
        assert base.count(before) == 1 and new.count(after) == 1, 'cannot make hunk unique'
        hunks.append({'before': before, 'after': after})
    return hunks


def regen():
    files = {p: _hunks(_git_base(p), (ROOT / p).read_bytes().decode('utf-8').replace('\r\n', '\n'))
             for p in _changed_paths()}
    data = {'base': BASE, 'description': 'clone-transition render layer (workstream R)', 'files': files}
    raw = (json.dumps(data, indent=1, sort_keys=True) + '\n').encode('utf-8')
    MANIFEST.write_bytes(raw)
    print('manifest', MANIFEST.name, len(files), 'files, sha256', hashlib.sha256(raw).hexdigest())


def _load():
    raw = MANIFEST.read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(raw).hexdigest() == MANIFEST_SHA256, ('clone transition allowance pin', hashlib.sha256(raw).hexdigest())
    data = json.loads(raw)
    assert data['base'] == BASE
    return data['files']


def reverse_clone_transition(source, path):
    """Peel the transition render layer off `source` (a covered file's current text). Paths not in the
    manifest, and sources that are already unchanged, pass through untouched."""
    # Chain: the engine layer (workstream E) covers a disjoint file set; peel it first (no-op for render-covered paths).
    from clone_transition_engine_pins import reverse_engine_layer
    source = reverse_engine_layer(source, path)
    if not MANIFEST.exists() or MANIFEST_SHA256.strip('0') == '':
        return source
    files = _load()
    key = path if path in files else next((p for p in files if p.endswith('/' + path) or p.endswith(path)), None)
    if key is None or '/' not in key:
        return source
    original = _git_base(key)
    if source == original:
        return source
    if not any(h['after'] in source for h in files[key]):
        return source   # already peeled by an outer layer (or never had this layer): older layers judge it
    expected = original
    for hunk in files[key]:
        assert expected.count(hunk['before']) == 1, ('clone transition baseline hunk', key)
        expected = expected.replace(hunk['before'], hunk['after'], 1)
    assert source == expected, ('clone transition layer: full feature source', key)
    for hunk in reversed(files[key]):
        assert source.count(hunk['after']) == 1, ('clone transition exact hunk', key)
        source = source.replace(hunk['after'], hunk['before'], 1)
    assert source == original, ('clone transition reversal must reproduce its exact baseline', key)
    return source


if __name__ == '__main__':
    if '--regen' in sys.argv:
        regen()
    else:
        files = _load()
        for p in files:
            cur = (ROOT / p).read_bytes().decode('utf-8').replace('\r\n', '\n')
            assert reverse_clone_transition(cur, p) == _git_base(p), p
        print('clone_transition_pins: PASS', len(files), 'files reverse to base')
