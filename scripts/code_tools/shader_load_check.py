#!/usr/bin/env python3
"""Fail-closed shader load/cache gate; --check and --selftest never build the viewer.

Epoch maintenance: increment SHADER_CACHE_EPOCH, append a matching digest to
shader_load_epochs.json, retaining existing entries. No automatic rebaseline flag.
Only native sources and the viewer's own .glsl tree are read. No external shaders.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
MGR = 'indra/llrender/llshadermgr.cpp'
HEADER = 'indra/llrender/llshadermgr.h'
GLSL = 'indra/llrender/llglslshader.cpp'
VIEWER = 'indra/newview/llviewershadermgr.cpp'
HERO = 'indra/newview/llheroprobemanager.cpp'
REGION = 'indra/newview/llviewerregion.cpp'
SHADERS = 'indra/newview/app_settings/shaders'
PINS = 'scripts/code_tools/shader_load_epochs.json'
SETTINGS = 'indra/newview/app_settings/settings.xml'
INPUTS = (MGR, HEADER, GLSL, VIEWER, HERO, REGION, SETTINGS, 'indra/llrender/llglslshader.h', 'indra/llrender/llshaderdiagnostics.h')
# Whole routines, including call/attachment order and all generated text. Header
# pinning also covers feature representation and inline helpers. The source-site
# inventory below rejects new injection/attachment sites outside these regions.
REGIONS = {
    MGR: ('LLShaderMgr::shaderLanguageVersion(', 'LLShaderMgr::prepareWeatherFeatures(',
          'LLShaderMgr::attachShaderFeatures(', 'LLShaderMgr::attachShaderFeaturesInternal(', 'LLShaderMgr::loadShaderFile(', 'LLShaderMgr::loadShaderFileInternal(',
          'LLShaderMgr::linkProgramObject(', 'LLShaderMgr::linkProgramObjectInternal(', 'LLShaderMgr::submitProgramLink(', 'LLShaderMgr::finishProgramLinkInternal(', 'LLShaderMgr::readShaderSource(', 'shader_source_has_main(',
          'shader_shared_source_fingerprint(', 'LLShaderMgr::hashShaderCacheFeatures(',
          'LLShaderMgr::initShaderCache(', 'LLShaderMgr::loadCachedProgramBinary(', 'LLShaderMgr::loadCachedProgramBinaryInternal(',
          'LLShaderMgr::saveCachedProgramBinary(', 'LLShaderMgr::configureShaderDiagnostics(',
          'LLShaderMgr::openShaderEquivalence(', 'LLShaderMgr::rejectProgramBinary(',
          'LLShaderMgr::ShaderAttemptScope::ShaderAttemptScope(', 'LLShaderMgr::ShaderAttemptScope::~ShaderAttemptScope(',
          'LLShaderMgr::beginShaderAttempt(', 'LLShaderMgr::snapshotShaderAttempt(',
          'LLShaderMgr::beginShaderStage(', 'LLShaderMgr::assembledShaderStage(',
          'LLShaderMgr::finishShaderStage(', 'LLShaderMgr::writeShaderRecord(',
          'LLShaderMgr::beginShaderPass(', 'LLShaderMgr::finishShaderPass(',
          'LLShaderMgr::traceAttachment(', 'LLShaderMgr::traceFeatureAttachment(',
          'LLShaderMgr::traceLink(', 'LLShaderMgr::injectShaderFailure(',
          'LLShaderMgr::injectShaderOverflow(', 'LLShaderMgr::finishShaderAttempt('),
    GLSL: ('LLGLSLShader::unloadInternal(', 'LLGLSLShader::createShader(', 'LLGLSLShader::createShaderInternal(', 'LLGLSLShader::prepareShaderInternal(', 'LLGLSLShader::submitShaderInternal(', 'LLGLSLShader::finishShaderInternal(', 'LLGLSLShader::attachVertexObject(', 'LLGLSLShader::attachVertexObjectInternal(',
           'LLGLSLShader::attachFragmentObject(', 'LLGLSLShader::attachFragmentObjectInternal(', 'LLGLSLShader::attachObject(', 'LLGLSLShader::attachObjectInternal(',
           'LLGLSLShader::attachObjects(', 'LLGLSLShader::mapAttributes(', 'LLGLSLShader::mapAttributesInternal(', 'LLGLSLShader::bindAttributes(', 'LLGLSLShader::readAttributes(',
           'LLGLSLShader::link(', 'LLGLSLShader::linkInternal(', 'LLGLSLShader::submitLink(', 'LLGLSLShader::finishLinkInternal(', 'LLGLSLShader::hash(', 'LLGLSLShader::hashWithEpoch('),
    VIEWER: ('LLViewerShaderMgr::LLViewerShaderMgr(', 'LLViewerShaderMgr::setShaders(', 'LLViewerShaderMgr::loadBasicShaders('),
}
CPP_TOKENS = re.compile(
    r'R"(?P<delimiter>[^\s()\\]{0,16})\([\s\S]*?\)(?P=delimiter)"'
    r'|"(?:\\[\s\S]|[^"\\\r\n])*"|\'(?:\\[\s\S]|[^\'\\\r\n])*\''
    r'|//[^\r\n]*|/\*[\s\S]*?\*/')


def mask(source):
    return CPP_TOKENS.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), source)


def function(source, name):
    start = source.index(name)
    masked = mask(source)
    brace = masked.index('{', start)
    depth = 1
    for end in range(brace + 1, len(masked)):
        depth += (masked[end] == '{') - (masked[end] == '}')
        if depth == 0:
            # Include the signature's return type/static qualifier too.
            line = source.rfind('\n', 0, start) + 1
            previous = source.rfind('\n', 0, max(line - 1, 0)) + 1
            if source[previous:line].strip().startswith('template<'):
                line = previous
            return source[line:end + 1]
    raise ValueError('unclosed function: ' + name)


def epoch(files):
    return int(re.search(r'SHADER_CACHE_EPOCH\s*=\s*(\d+)\s*;', files[HEADER])[1])


def region_digest(files):
    digest = hashlib.sha256()
    for path, names in REGIONS.items():
        for name in names:
            digest.update((path + '\n' + name + '\n' + function(files[path], name) + '\n').encode())
    for path in (HEADER, 'indra/llrender/llglslshader.h', 'indra/llrender/llshaderdiagnostics.h'):
        source = re.sub(r'(SHADER_CACHE_EPOCH\s*=\s*)\d+', r'\1EPOCH', files[path])
        digest.update((path + '\n' + source).encode())
    return digest.hexdigest()


def has_main(source):
    return bool(re.search(r'\bmain\s*\(', re.sub(r'//[^\r\n]*|/\*[\s\S]*?\*/', ' ', source)))


def feature_fields(source):
    """Parse every declaration; unfamiliar syntax/types require explicit review."""
    body = mask(function(source, 'class LLShaderFeatures'))
    body = body[body.index('{') + 1:body.rindex('}')].replace('public:', '')
    fields = []
    for declaration in body.split(';'):
        if not declaration.strip():
            continue
        match = re.fullmatch(r'\s*(S32|bool)\s+(\w+)\s*=\s*(?:0|false)\s*', declaration)
        if not match:
            raise ValueError('unreviewed LLShaderFeatures declaration: ' + declaration.strip())
        fields.append(match[2])
    if not fields:
        raise ValueError('LLShaderFeatures has no parsed fields')
    return fields


def dependency_errors(files, shaders):
    errors = []
    regions = function(files[MGR], 'LLShaderMgr::attachShaderFeaturesInternal(') + function(files[VIEWER], 'LLViewerShaderMgr::loadBasicShaders(')
    names = set(re.findall(r'"([^"\n]+\.glsl)"', regions))
    for name in sorted(names):
        matches = [(p, s) for p, s in shaders.items() if p.endswith('/' + name)]
        if not matches:
            errors.append('indirect dependency missing: ' + name)
        for path, source in matches:
            if has_main(source):
                errors.append('indirect dependency defines main(): ' + path)
    for path, source in shaders.items():
        source = re.sub(r'//[^\r\n]*|/\*[\s\S]*?\*/', ' ', source)
        if re.search(r'^\s*#\s*include\b', source, re.M):
            errors.append('loadShaderFile has no #include support: ' + path)
        if re.search(r'^\s*#\s*define\s+\w+[^\n]*\bmain\b', source, re.M):
            errors.append('macro-generated entry point requires dependency review: ' + path)
    return errors


def source_errors(files, shaders, pins):
    errors = []
    region = function(files[REGION], 'LLViewerRegion::setSimulatorFeatures(')
    if re.search(r'\b(?:setShaders|clearShaderCache)\s*\(', mask(region)):
        errors.append('simulator capabilities must never reload shaders or clear their cache')
    for value in ('enabled', 'false'):
        if 'gSavedSettings.setBOOL("GLTFEnabled", %s);' % value not in region:
            errors.append('simulator GLTF capability setting must still be updated: ' + value)
    classifier = function(files[MGR], 'shader_source_has_main(')
    if re.search(r'\b(?:regex\w*|throw|try|catch)\b', mask(classifier)) or '#include <regex>' in files[MGR]:
        errors.append('shader main classifier must be a scanner without regex or exceptions')
    hero = files[HERO]
    if re.search(r'\b(?:setShaders|clearShaderCache)\s*\(', mask(hero)):
        errors.append('hero manager must never reload shaders or clear their cache')
    if 'RenderHeroProbeClearShaderCache' in hero:
        errors.append('retired hero cache setting must have no readers')
    update = function(hero, 'LLHeroProbeManager::update(')
    initialization = function(update, 'if (!mInitialized &&')
    guard = initialization[:initialization.index('{')]
    if '!mHeroVOList.empty()' not in guard or 'STATE_PRECACHE' not in guard:
        errors.append('buffer rebuild must wait for a registered mirror after precache')
    if update.index(initialization) > update.index('initReflectionMaps();'):
        errors.append('mirror init must precede probe references/early no-visible-mirror return')
    for call in ('gPipeline.releaseGLBuffers();', 'gPipeline.createGLBuffers();', 'mInitialized = true;', 'LL_INFOS("MirrorInit")'):
        if call not in initialization:
            errors.append('missing one-shot mirror initialization: ' + call)
    for call in ('releaseGLBuffers', 'createGLBuffers'):
        if len(re.findall(r'\b' + call + r'\s*\(', mask(hero))) != 1:
            errors.append('all hero buffer rebuild calls must stay inside the mirror guard')
    if 'return;' in mask(initialization):
        errors.append('mirror init must restore scratch targets before renderProbes this frame')
    init = function(files[MGR], 'LLShaderMgr::initShaderCache(')
    if 'RenderShaderCacheVersion' in files[MGR] + files[VIEWER] or 'old_cache_version' in init:
        errors.append('settings must not decide shader cache validity')
    if re.search(r'LLVersionInfo|current_cache_version|whole.tree|version.only', init + function(files[VIEWER], 'LLViewerShaderMgr::setShaders('), re.I):
        errors.append('viewer version/tree fingerprint must not decide metadata validity')
    if 'metadata_readable && in_data["version"].asUUID() == mShaderCacheVersion' not in init:
        errors.append('cache keep decision must use readable metadata and schema only')
    if 'else if (!second_instance)' not in init:
        errors.append('second instance must never purge cache')
    if 'enabled && mShaderCacheSourceValid' not in init or 'if (!mShaderCacheScanAttempted)' not in init:
        errors.append('failed source scan must disable cache for the entire run')
    hashed = function(files[GLSL], 'LLGLSLShader::hashWithEpoch(')
    if function(files[GLSL], 'LLGLSLShader::hash(').strip() != 'LLUUID LLGLSLShader::hash()\n{\n    return hashWithEpoch(LLShaderMgr::SHADER_CACHE_EPOCH);\n}':
        errors.append('production hash must use the declared cache epoch')
    for item in ('mSharedShaderFingerprint', 'hash_obj.update(&epoch, sizeof(epoch));', 'mReservedAttribs', 'mShaderCacheFeatureHash'):
        if item not in hashed:
            errors.append('program hash missing input/fail-safe: ' + item)
    declared = feature_fields(files['indra/llrender/llglslshader.h'])
    snapshot = function(files[MGR], 'LLShaderMgr::snapshotShaderAttempt(')
    dumped = re.findall(r'features\["(\w+)"\] = shader->mFeatures\.\1;', snapshot)
    if dumped != declared:
        errors.append('equivalence dump must include every LLShaderFeatures field in declaration order')
    if '&& !mShaderInjectionActive' not in init:
        errors.append('failure injection must disable binary cache initialization')
    configuration = function(files[MGR], 'LLShaderMgr::configureShaderDiagnostics(')
    if 'mShaderCacheEnabled = false;' not in configuration:
        errors.append('failure injection must also disable an already initialized binary cache')
    try:
        items = list(ET.fromstring(files[SETTINGS]).find('map'))
        for name, kind, default in (('RenderShaderDumpEquivalence', 'Boolean', '0'),
                                    ('RenderShaderInjectFail', 'String', '')):
            matches = [value for key, value in zip(items[::2], items[1::2]) if key.text == name]
            if len(matches) != 1:
                errors.append('diagnostic setting must be declared exactly once: ' + name)
                continue
            entries = list(matches[0])
            metadata = {key.text: value.text or '' for key, value in zip(entries[::2], entries[1::2])}
            if metadata.get('Type') != kind or metadata.get('Value') != default or metadata.get('Persist') != '0':
                errors.append('invalid diagnostic setting type/default/Persist: ' + name)
    except (ET.ParseError, TypeError):
        errors.append('malformed diagnostic settings document')
    feeds = re.findall(r'hash_obj\.update\(&mFeatures\.(\w+), sizeof\(mFeatures\.\1\)\);', hashed)
    if feeds != declared or re.search(r'&mFeatures\s*[,)]', hashed):
        errors.append('LLShaderFeatures must hash every declared field exactly once in declaration order')
    if 'disableShaderCache' in mask(hashed):
        errors.append('program root failure must not disable the process cache')
    if ('mShaderCacheEligible = true;' not in hashed or
            hashed.count('mShaderCacheEligible = false;') != 2):
        errors.append('hash must reset program eligibility and reject missing/unreadable roots')
    for name, guard in (
            ('loadCachedProgramBinary', 'if (!mShaderCacheEnabled || !shader->mShaderCacheEligible) return false;'),
            ('saveCachedProgramBinary', 'if (!mShaderCacheEnabled || mShaderCacheReadOnly || !shader->mShaderCacheEligible) return true;')):
        method = function(files[MGR], 'LLShaderMgr::' + name + ('Internal(' if name == 'loadCachedProgramBinary' else '('))
        if guard not in method:
            errors.append(name + ' must check global and per-program state after hash')
    reader = function(files[MGR], 'LLShaderMgr::readShaderSource(')
    if 'size == 0' in mask(reader):
        errors.append('empty shader sources are valid content')
    basic = function(files[VIEWER], 'LLViewerShaderMgr::loadBasicShaders(')
    if not basic.rstrip().endswith('hashShaderCacheFeatures();\n\n    return std::string();\n}'):
        errors.append('shared assembled-object digest must be frozen before program roots')
    scan = function(files[MGR], 'shader_shared_source_fingerprint(')
    for item in ('fsyspath(prefix)', 'recursive_directory_iterator', 'std::sort', 'shader_source_has_main(source)', 'hash_obj.update(source)', 'if (ec || files.empty()) return false;'):
        if item not in scan:
            errors.append('incomplete fail-closed shared-source scan: ' + item)
    if 'skip_permission_denied' in mask(scan):
        errors.append('shared-source scan must surface inaccessible subtrees')
    errors.extend(dependency_errors(files, shaders))
    current = str(epoch(files))
    if pins.get(current) != region_digest(files):
        errors.append('injected/link/cache C++ regions changed: bump SHADER_CACHE_EPOCH and append its reviewed digest')
    return errors


def load(root):
    files = {p: (root / p).read_text(encoding='utf-8') for p in INPUTS}
    shaders = {p.relative_to(root / SHADERS).as_posix(): p.read_text(encoding='utf-8')
               for p in sorted((root / SHADERS).rglob('*.glsl'))}
    if not shaders:
        raise ValueError('empty shader tree')
    pins = json.loads((root / PINS).read_text(encoding='utf-8'))
    return files, shaders, pins


# Find sensitive identifiers before trying to interpret C++ template syntax.
# This also catches callable aliases and parenthesized/nested template arguments.
INJECTION_PATTERN = r'\b(?:glShaderSource|glShaderBinary|glSpecializeShader|glCompileShader|glCreateShader|glCreateProgram|glAttachShader|glDetachShader|glLinkProgram|glBindAttribLocation|glBindFragDataLocation(?:Indexed)?|glTransformFeedbackVaryings|glProgramParameteri|glProgramBinary|loadShaderFile(?:Internal)?|attachVertexObject(?:Internal)?|attachFragmentObject(?:Internal)?|attachObject(?:Internal)?|attachObjects)\b'
# Existing inventory/object attachment APIs happen to share the shader method
# names. Whitelist exact non-shader expressions, never a whole native source.
NON_SHADER_ATTACH = {
 'indra/newview/llvoavatar.cpp': ('attachObject(childp)', 'attachObject(cur_attachment)',
     'LLVOAvatar::attachObject(LLViewerObject *viewer_object)'),
 'indra/newview/llvoavatarself.cpp': ('LLVOAvatarSelf::attachObject(LLViewerObject *viewer_object)',
     'LLVOAvatar::attachObject(viewer_object)'),
 'indra/newview/llinventorypanel.cpp': ('LLInventoryPanel::attachObject(const LLSD& userdata)',
     'LLViewerAttachMenu::attachObjects(items, userdata.asString())',
     'boost::bind(&LLInventoryPanel::attachObject, this, _2)'),
 'indra/newview/llwearableitemslist.cpp': ('boost::bind(LLViewerAttachMenu::attachObjects, ids, _2)',),
 'indra/newview/llviewerattachmenu.cpp': ('LLViewerAttachMenu::attachObjects(const uuid_vec_t& items, const std::string& joint_name)',),
}


def unpinned_sites(path, source):
    for name in REGIONS.get(path, ()):
        source = source.replace(function(source, name), '')
    source = mask(source)
    # Explicit instantiation declarations have no executable body. Do not
    # confuse these with calls to the newly pinned compile-time entry points.
    source = re.sub(r'^template (?:bool|void|GLuint) LL(?:ShaderMgr|GLSLShader)::\w+Internal<(?:true|false)>\([^;{}]*\);$',
                    '', source, flags=re.M)
    if path == 'indra/llrender/llgl.cpp':
        # Exact GL entry-point storage declarations and loader assignments,
        # never an executable invocation (including calls via a template).
        source = re.sub(r'^PFNGL[A-Z0-9]+PROC\s+gl\w+ = nullptr;$', '', source, flags=re.M)
        source = re.sub(r'^    (gl\w+) = \(PFNGL[A-Z0-9]+PROC\)LL_GET_PROC_ADDRESS\(\s*\);$', '', source, flags=re.M)
    for expression in NON_SHADER_ATTACH.get(path, ()):
        source = source.replace(expression, '')
    return bool(re.search(INJECTION_PATTERN, source))


def injection_site_errors(root, files):
    # llbakingShaderMgr is the appearance baker, never initializes binary caching.
    # git, not rg: the prebuild shell has no ripgrep. Tracked plus new files,
    # the same set rg would see, minus the private reference trees.
    found = subprocess.run(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '--', 'indra/*.cpp'],
                           cwd=root, capture_output=True, text=True, encoding='utf-8')
    if found.returncode:
        return ['shader injection call-site scan failed: ' + found.stderr]
    errors = []
    for path in found.stdout.splitlines():
        path = path.replace('\\', '/')
        if (path == 'indra/llappearanceutility/llbakingshadermgr.cpp'
                or 'Reeelight' in path or 'MartysMods_RELIGHT' in path):
            continue
        source = files.get(path)
        if source is None:
            full = root / path
            if not full.is_file():
                continue
            source = full.read_text(encoding='utf-8', errors='replace')
        if re.search(INJECTION_PATTERN, source) and unpinned_sites(path, source):
            errors.append('new unpinned shader injection/attachment call site: ' + path)
    return errors


def check(root=ROOT):
    try:
        files, shaders, pins = load(root)
        errors = source_errors(files, shaders, pins) + injection_site_errors(root, files)
        # Existing entries are immutable: updating a digest at the same epoch is
        # forbidden. Only local HEAD is read; no remotes, upstream or log scans.
        old = subprocess.run(['git', 'show', 'HEAD:' + PINS], cwd=root, capture_output=True, text=True)
        if old.returncode == 0:
            previous = json.loads(old.stdout)
            if any(pins.get(k) != v for k, v in previous.items()):
                errors.append('existing epoch pins are immutable; append a new epoch')
        return errors
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return ['shader load gate failed closed: ' + str(exc)]


def selftest(quiet=False):
    files, shaders, pins = load(ROOT)
    mutations = [
        ('simulator GLTF shader reload', REGION, 'gSavedSettings.setBOOL("GLTFEnabled", enabled);',
         'gSavedSettings.setBOOL("GLTFEnabled", enabled); LLViewerShaderMgr::instance()->setShaders();'),
        ('hero shader reload', HERO, 'gPipeline.createGLBuffers();', 'LLViewerShaderMgr::instance()->setShaders();'),
        ('hero cache clear', HERO, 'gPipeline.createGLBuffers();', 'LLViewerShaderMgr::instance()->clearShaderCache();'),
        ('login buffer rebuild without mirror', HERO, '!mHeroVOList.empty() && ', ''),
        ('settings-driven purge', MGR, 'metadata_readable && in_data', 'gSavedSettings.getString("RenderShaderCacheVersion").empty() && metadata_readable && in_data'),
        ('viewer build purge', VIEWER, 'initShaderCache(shader_cache_enabled,', 'LLVersionInfo::instance().getVersion(); initShaderCache(shader_cache_enabled,'),
        ('whole-tree purge', MGR, 'metadata_readable && in_data', 'whole_tree_hash == current_cache_version && metadata_readable && in_data'),
        ('shared file omitted', MGR, 'if (!shader_source_has_main(source))', 'if (relative != "class1/deferred/globalF.glsl" && !shader_source_has_main(source))'),
        ('injected text changed without epoch', MGR, '#define FRAGMENT_SHADER 1', '#define FRAGMENT_SHADER 2'),
        ('scan failure serves old binaries', MGR, 'enabled && mShaderCacheSourceValid', 'enabled'),
        ('second instance purge', MGR, 'else if (!second_instance)', 'else'),
        ('short read accepted', MGR, 'stream.gcount() != static_cast<std::streamsize>(size)', 'false'),
        ('unconditional extra buffer rebuild', HERO, '    initReflectionMaps();', '    gPipeline.releaseGLBuffers(); initReflectionMaps();'),
        ('attribute names omitted', GLSL, 'manager->mReservedAttribs', 'std::vector<std::string>{}'),
    ]
    failures = []
    for label, path, old, new in mutations:
        assert old in files[path], 'fixture anchor missing: ' + label
        changed = dict(files)
        changed[path] = changed[path].replace(old, new, 1)
        try:
            rejected = source_errors(changed, shaders, pins)
        except (ValueError, KeyError):
            rejected = ['malformed production contract']
        if not rejected:
            failures.append('SURVIVING FIXTURE: ' + label)
        elif not quiet:
            print('rejected:', label)
    changed = dict(shaders)
    indirect = next(p for p in changed if p.endswith('/deferred/globalF.glsl'))
    changed[indirect] += '\nvoid main() {}\n'
    if not dependency_errors(files, changed):
        failures.append('SURVIVING FIXTURE: indirect stage root')
    elif not quiet:
        print('rejected: indirect stage root')
    for label, call in (
            ('unpinned direct GL attachment', 'glAttachShader(program, stage);'),
            ('unpinned wrapper attachment', 'shader.attachObjects(stages, 2);'),
            ('unpinned indexed fragment output', 'glBindFragDataLocationIndexed(program, 0, 1, "color");'),
            ('unpinned diagnostic attachment', 'shader.attachVertexObjectInternal<true>("feature.glsl");'),
            ('unpinned parenthesized attachment', 'shader.attachVertexObjectInternal<(true)>("feature.glsl");'),
            ('unpinned parenthesized load', 'mgr.loadShaderFileInternal<(false)>(file, level, type, nullptr, 0);'),
            ('unpinned diagnostic stage load', 'mgr.loadShaderFileInternal<false>(file, level, type, nullptr, 0);')):
        if not unpinned_sites('fixture.cpp', 'void added(){' + call + '}'):
            failures.append('SURVIVING FIXTURE: ' + label)
        elif not quiet:
            print('rejected:', label)
    # Refresh the epoch digest in these structural fixtures so they must fail
    # their own contracts, independently of the injection pin.
    structural = [
        ('wrong production epoch', GLSL, 'return hashWithEpoch(LLShaderMgr::SHADER_CACHE_EPOCH);', 'return hashWithEpoch(0);'),
        ('epoch feed omitted', GLSL, '    hash_obj.update(&epoch, sizeof(epoch));', ''),
        ('feature field omitted', GLSL,
         '    hash_obj.update(&mFeatures.hasActorFxShadow, sizeof(mFeatures.hasActorFxShadow));', ''),
        ('new feature field omitted', 'indra/llrender/llglslshader.h',
         '    bool hasActorFxShadow = false;', '    bool hasActorFxShadow = false;\n    bool futureFeature = false;'),
        ('feature raw padding hashed', GLSL,
         '    hash_obj.update(&mFeatures.hasActorFxShadow, sizeof(mFeatures.hasActorFxShadow));',
         '    hash_obj.update(&mFeatures, sizeof(mFeatures));'),
        ('root failure disables all programs', GLSL, 'mShaderCacheEligible = false;', 'LLShaderMgr::instance()->disableShaderCache();'),
        ('binary load uses partial key', MGR, ' || !shader->mShaderCacheEligible) return false;', ') return false;'),
        ('binary save uses partial key', MGR, ' || !shader->mShaderCacheEligible) return true;', ') return true;'),
        ('binary load ignores global disable', MGR, '!mShaderCacheEnabled || !shader->mShaderCacheEligible', '!shader->mShaderCacheEligible'),
        ('binary save ignores global disable', MGR, '!mShaderCacheEnabled || mShaderCacheReadOnly || !shader->mShaderCacheEligible', 'mShaderCacheReadOnly || !shader->mShaderCacheEligible'),
        ('empty source rejected', MGR, 'if (ec || size >', 'if (ec || size == 0 || size >'),
        ('injection enables cache', MGR, ' && !mShaderInjectionActive;', ';'),
        ('injection retains live cache', MGR, '        mShaderCacheEnabled = false;', ''),
        ('feature dump omits field', MGR, '    features["hasActorFxShadow"] = shader->mFeatures.hasActorFxShadow;', ''),
    ]
    for name in ('RenderShaderDumpEquivalence', 'RenderShaderInjectFail'):
        declaration = re.search(r'<key>' + name + r'</key>\s*<map>[\s\S]*?</map>', files[SETTINGS])[0]
        structural.append((name + ' persists', SETTINGS, declaration,
                           declaration.replace('<key>Persist</key><integer>0</integer>',
                                               '<key>Persist</key><integer>1</integer>')))
    for label, path, old, new in structural:
        assert old in files[path], 'fixture anchor missing: ' + label
        changed = dict(files)
        changed[path] = changed[path].replace(old, new, 1)
        reviewed = dict(pins)
        reviewed[str(epoch(changed))] = region_digest(changed)
        if not source_errors(changed, shaders, reviewed):
            failures.append('SURVIVING FIXTURE: ' + label)
        elif not quiet:
            print('rejected:', label)
    # An epoch bump with an appended matching pin is the explicit escape hatch;
    # simply changing the epoch alone must also fail.
    changed = dict(files)
    changed[MGR] = changed[MGR].replace('#define FRAGMENT_SHADER 1', '#define FRAGMENT_SHADER 2', 1)
    changed[HEADER] = changed[HEADER].replace('SHADER_CACHE_EPOCH = %d;' % epoch(files), 'SHADER_CACHE_EPOCH = %d;' % (epoch(files) + 1))
    if not source_errors(changed, shaders, pins):
        failures.append('unreviewed epoch bump accepted')
    advanced = dict(pins)
    advanced[str(epoch(changed))] = region_digest(changed)
    if source_errors(changed, shaders, advanced):
        failures.append('reviewed epoch bump not accepted')
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
    elif not quiet:
        print('shader_load_check selftest: 38 planted regressions rejected; epoch transition checked')
    return 1 if failures else 0


def native_check(root=ROOT):
    # Compiles extracted production scanner + init decision; service shells only.
    from shader_load_fixture import run
    run(root)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument('--check', action='store_true')
    modes.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    errors = check()
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    native_check()
    print('shader_load_check: PASS (schema, dependency coverage, epoch, native policy and scan fixtures)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
