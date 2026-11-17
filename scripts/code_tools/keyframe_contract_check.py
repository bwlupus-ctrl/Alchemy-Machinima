"""WP0 location-aware contracts and semantic source negatives (no viewer build)."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / 'indra/newview'
HEADERS = ('alkeyframecore alkeyframetransport alkeyframehistoryadapter alkeyframefit '
           'alkeyframecamera alkeyframeplayer alkeyframelibrary alcleanframe '
           'alkeyframeinputgate alkeyframetimeline alpanelkeyframes alkeyframeoverlay '
           'altoolkeyframeedit').split()
PURE = ('alkeyframecore alkeyframetransport alkeyframefit alcleanframe '
        'alkeyframeinputgate alkeyframehistoryadapter').split()


def assertions(files):
    errors = []
    for name in HEADERS:
        if name + '.h' not in files:
            errors.append('C0 headers present')
    allowed = {name + '.h' for name in PURE} | {'alundosequence.h', 'stdtypes.h'}
    for name in PURE:
        includes = re.findall(r'^\s*#include\s+"([^"]+)"', files[name + '.h'], re.M)
        if any(i not in allowed for i in includes):
            errors.append('C1 std-only headers')
    enum = re.search(r'enum class ECameraOwner\s*\{([^}]+)\}', files['alcameradispatch.h']).group(1)
    owners = re.findall(r'\b[A-Z][A-Z_]+\b', enum)
    if owners != ['PILOT', 'RECORDER', 'PATH', 'CINE', 'FLYCAM', 'AGENT', 'KEYFRAME']:
        errors.append('C2 append preserves enum values')
    loop = re.search(r'for \(ECameraOwner owner : \{([^}]+)\}', files['handover']).group(1)
    if set(re.findall(r'ECameraOwner::(\w+)', loop)) != set(owners) - {'FLYCAM'}:
        errors.append('C3 foreign owner enumeration')
    if not re.search(r'bool hasRawRenderDebugFeatureMask\(const U32 mask\) const;', files['pipeline.h']):
        errors.append('C4 raw declaration only')
    cmake = files['CMakeLists.txt']
    if any(cmake.count('    ' + h + '.h\n') != 1 for h in HEADERS):
        errors.append('C5 CMake headers once')
    for stem in ('alkeyframecore', 'alkeyframecodec', 'alkeyframehistoryadapter', 'alkeyframefit'):
        if cmake.count('    ' + stem + '.cpp\n') != 1:
            errors.append('C5 CMake sources once')
    return errors


def check():
    files = {p.name: p.read_text(encoding='utf-8') for p in [*(NV / (h + '.h') for h in HEADERS),
             NV / 'alcameradispatch.h', NV / 'pipeline.h', NV / 'CMakeLists.txt']}
    files['handover'] = (NV / 'tests/alcineflycamhandover_test.cpp').read_text(encoding='utf-8')
    assert not assertions(files), assertions(files)
    print('PASS C0-C5 keyframe contracts')
    mutations = [
        ('C1 std-only headers', 'alkeyframecore.h', '#include <array>', '#include "llvector4.h"\n#include <array>'),
        ('C2 append preserves enum values', 'alcameradispatch.h', '    AGENT,\n    KEYFRAME', '    KEYFRAME,\n    AGENT'),
        ('C3 foreign owner enumeration', 'handover', ', ECameraOwner::KEYFRAME}', '}')]
    for assertion, file, before, after in mutations:
        assert files[file].count(before) == 1, (file, before)
        planted = dict(files); planted[file] = planted[file].replace(before, after)
        failures = assertions(planted)
        assert assertion in failures, (assertion, failures)
        print('PASS negative [src]: FAIL ' + assertion)
    assert not assertions(files)
    print('PASS clean contracts after 3 planted negatives')


if __name__ == '__main__':
    check()
