"""Exact additive allowances above the immutable six-clone-FX integration.

The older manifests and their digests stay unchanged.  Each new layer first
reconstructs its predecessor from the historical commit, then reverses only
its own exact hunks and requires equality with that entire predecessor.
"""
from pathlib import Path
import hashlib
import json
import subprocess


ROOT = Path(__file__).resolve().parents[2]
BASE = '66e124787e6'
FX_MOVER_SHA256 = '9ccf4c83774ff89260a93adb6b76706773b88da5bc5389b2fa85d61bc48fa846'
FX_SHADER_SHA256 = '37376983ab6949160718894018f22b14741f7d204c6bca8705461869b04f0034'
REST_MOVER_SHA256 = '0f9560eecd467b266d31d989acb9fd0f31e3901e553a9ee97ebf06ef830c25f0'
REST_SHADER_SHA256 = 'f2af1cd23e27c3d33e983a441a5c09f300ac5f803a3bb4e2d7bdd6a1cab4fa54'
REST_FIX1_SHA256 = 'dc1f9b656526c09b0b74eba1fab2041cf5aea6b2b3844fed8648e619e50a34d3'


def _manifest(filename, digest):
    raw = (ROOT / 'scripts/code_tools' / filename).read_bytes().replace(b'\r\n', b'\n')
    assert hashlib.sha256(raw).hexdigest() == digest, (filename, 'allowance pin')
    data = json.loads(raw)
    assert data['base'] == BASE, (filename, 'immutable base')
    return data


def _reverse(source, path, filename, digest, predecessor, predecessor_digest, prefix=''):
    from clone_transition_pins import reverse_clone_transition  # newest layer first (clone transitions, render)
    source = reverse_clone_transition(source, path)
    layer = _manifest(filename, digest)
    prior = _manifest(predecessor, predecessor_digest)
    assert layer['integration_predecessors'] == [predecessor], (filename, 'layer order')
    for name, expected in layer.get('new_files', {}).items():
        actual = (ROOT / prefix / name).read_text(encoding='utf-8')
        # The initial rest-frame helper remains immutable in that manifest.
        # Peel its reviewed replacement exactly before testing the old pin.
        if filename == 'clone_rest_frame_mover_hunks.json':
            fix = _manifest('clone_rest_frame_fix1_hunks.json', REST_FIX1_SHA256)
            assert fix['integration_predecessors'] == [filename], ('clone_rest_frame_fix1_hunks.json', 'layer order')
            assert set(fix['new_file_updates']) == set(layer['new_files']), ('clone_rest_frame_fix1_hunks.json', 'exact file set')
            update = fix['new_file_updates'][name]
            assert update['before'] == expected, ('clone_rest_frame_fix1_hunks.json', 'exact predecessor', name)
            assert actual == update['after'], ('clone_rest_frame_fix1_hunks.json', 'exact new file', name)
            actual = update['before']
        assert actual == expected, (filename, 'exact new file', name)
    if path not in layer['files']:
        return source
    original = subprocess.check_output(['git', 'show', BASE + ':' + prefix + path], cwd=ROOT).decode('utf-8').replace('\r\n', '\n')
    for hunk in prior['files'][path]:
        assert original.count(hunk['before']) == 1, (predecessor, 'baseline hunk', path)
        original = original.replace(hunk['before'], hunk['after'], 1)
    if source == original:
        return source
    # Check both directions against complete files.  An allowance may neither
    # introduce an ambiguous hunk nor conceal any unrelated source edit.
    expected = original
    for hunk in layer['files'][path]:
        assert expected.count(hunk['before']) == 1, (filename, 'baseline hunk', path)
        expected = expected.replace(hunk['before'], hunk['after'], 1)
    assert source == expected, (filename, 'full feature source', path)
    for hunk in reversed(layer['files'][path]):
        assert source.count(hunk['after']) == 1, (filename, 'exact feature hunk', path)
        source = source.replace(hunk['after'], hunk['before'], 1)
    assert source == original, (filename, 'reversal must reproduce its exact baseline', path)
    return source


def reverse_rest_frame_mover(source, name):
    return _reverse(source, name, 'clone_rest_frame_mover_hunks.json', REST_MOVER_SHA256,
                    'clone_fx_mover_hunks.json', FX_MOVER_SHA256, 'indra/newview/')


def reverse_rest_frame_shader(source, path):
    return _reverse(source, path, 'clone_rest_frame_shader_hunks.json', REST_SHADER_SHA256,
                    'clone_fx_shader_hunks.json', FX_SHADER_SHA256)
