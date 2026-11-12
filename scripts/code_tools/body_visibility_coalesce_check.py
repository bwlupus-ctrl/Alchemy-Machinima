#!/usr/bin/env python3
"""Gate for the coalesced body-visibility walk (AlchemyPerfCoalescedBodyVisibility).

Usage::

    python scripts/code_tools/body_visibility_coalesce_check.py            (check + selftest)
    python scripts/code_tools/body_visibility_coalesce_check.py --check
    python scripts/code_tools/body_visibility_coalesce_check.py --selftest

director_kit_gen.py --check runs both (run_check + selftest), so prebuild_guard reaches it.

LLVOAvatar::updateMeshVisibility walks every face of every attachment to decide
which system body parts are hidden. Attachment texture-entry callbacks used to
run it once per changed face; they now mark the avatar
(requestMeshVisibilityUpdate) and one walk runs at the frame flush or at the
first reader, whichever comes first.

Two halves, both must pass (exit 1 on any finding):

1. Source pins. Every caller of the walk is listed here with its decision
   (immediate or coalesced); a new caller is a finding until it is audited.
   Every reader of the joints' visibility flag and every other writer of it
   flushes a pending walk first; the frame flush sits after the image callbacks
   and before the Prism view / state sort / geometry passes; a dead or deleted
   avatar clears its mark; the setting is a persisted Boolean, default on; the
   new code allocates nothing.
2. Native fixture (body_visibility_coalesce_fixture.cpp) with the REAL
   production methods spliced in: coalesced == immediate over scripted
   sessions, immediate paths stay immediate, switch off = one walk per request,
   live toggle, delete, no work when clean, walk-before-reader order.

Planted negatives: each one changes one production line (or one source pin) and
must be caught, by the fixture test or the source pin named next to it.

Standard library + native_check_tools (MSVC). Exit 2 for missing inputs.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import native_check_tools as native  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
NEWVIEW = ROOT / "indra" / "newview"
SETTING = "AlchemyPerfCoalescedBodyVisibility"

SOURCES = {
    "avatar_cpp": NEWVIEW / "llvoavatar.cpp",
    "avatar_h": NEWVIEW / "llvoavatar.h",
    "object_cpp": NEWVIEW / "llviewerobject.cpp",
    "display_cpp": NEWVIEW / "llviewerdisplay.cpp",
    "panel_cpp": NEWVIEW / "llpanelvolume.cpp",
    "jointmesh_cpp": NEWVIEW / "llviewerjointmesh.cpp",
    "joint_cpp": NEWVIEW / "llviewerjoint.cpp",
    "avatarjoint_h": ROOT / "indra" / "llappearance" / "llavatarjoint.h",
    "settings": NEWVIEW / "app_settings" / "settings.xml",
}

# Every direct call of the walk, by file and enclosing function, with the
# audited decision. Anything not listed is a finding.
WALK_CALLERS = {
    ("avatar_cpp", "resetSkeleton"): 1,                 # immediate: jellydoll default appearance rebuild
    ("avatar_cpp", "attachObject"): 1,                  # immediate: attach
    ("avatar_cpp", "detachObject"): 1,                  # immediate: detach
    ("avatar_cpp", "applyParsedAppearanceMessage"): 1,  # immediate: appearance / rebake / creation / clone copy
    ("avatar_cpp", "updateOverallAppearance"): 1,       # immediate: normal / jellydoll / invisible change
    ("avatar_cpp", "requestMeshVisibilityUpdate"): 1,   # the switch-off / dead-avatar path
    ("avatar_cpp", "flushPendingMeshVisibility"): 1,    # the frame walk
    ("avatar_h", "flushMeshVisibilityIfDirty"): 1,      # the reader / writer guard
    ("panel_cpp", "getState"): 1,                       # immediate: build floater refresh on own attachment
    ("panel_cpp", "onCommitAnimatedMeshCheckbox"): 1,   # immediate: user toggles Animated Mesh
}
# The coalesced requests: only the attachment texture-entry callback.
REQUEST_CALLERS = {("object_cpp", "updateAvatarMeshVisibility"): 1}
# Its callers (all inside llviewerobject.cpp): the three texture-entry setters.
CALLBACK_CALLERS = {("object_cpp", "setTE"): 1, ("object_cpp", "setTEImage"): 1, ("object_cpp", "setTETextureCore"): 1}
# Functions that read the joints' visibility (through LLViewerJoint::render ->
# LLViewerJointMesh::drawShape, or isActorGhostReplayReady) or write it, and so
# must flush a pending walk first.
GUARDED = ("renderSkinned", "renderTransparent", "renderRigid", "isActorGhostSystemReplayReady",
           "hideHair", "hideSkirt")
# Functions allowed to fetch a body mesh joint (getViewerJoint). The two
# unguarded ones never reach mVisible: updateMeshData sizes / fills buffers,
# uploadActorGhostPalette uploads matrices.
JOINT_USERS = {"updateMeshData", "renderSkinned", "renderTransparent", "renderRigid",
               "uploadActorGhostPalette", "isActorGhostSystemReplayReady", "getViewerJoint"}
# Every mention of the joints' visibility flag (LLAvatarJoint::mVisible, protected)
# in the whole joint class hierarchy: any class derived from LLAvatarJoint, in
# any file of llappearance / newview that declares one or defines a member of
# one. Two writers (init, setVisible), two readers (both reached only through
# the guarded LLVOAvatar functions), one declaration. A new reader anywhere in
# the hierarchy, or an accessor that would let other code read it, is a finding.
JOINT_FLAG_SITES = {
    ("llavatarjoint.h", "<header>"): 1,                                  # the declaration
    ("llavatarjoint.cpp", "LLAvatarJoint::init"): 1,                     # writer: default true
    ("llavatarjoint.cpp", "LLAvatarJoint::setVisible"): 1,               # writer: the walk / hideHair / hideSkirt
    ("llviewerjointmesh.cpp", "LLViewerJointMesh::isActorGhostReplayReady"): 1,  # reader, guarded
    ("llviewerjointmesh.cpp", "LLViewerJointMesh::drawShape"): 1,        # reader, guarded
}
JOINT_DIRS = (ROOT / "indra" / "llappearance", NEWVIEW)
ALLOCATING = ("new ", "push_back", "emplace", "std::vector", "std::string", "std::map", "LLSD", "insert(",
              "resize(", "reserve(", "make_shared", "make_unique", "malloc")


def read(path):
    return path.read_text(encoding="utf-8", errors="replace")


def strip_comments(text):
    """Blank comments and string contents, keeping every offset."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(c + " " * (j - i - 1) + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_span(text, signature):
    """(start, end) of the one function whose definition starts a line with `signature`."""
    clean = strip_comments(text)
    starts = [m.start() for m in re.finditer(r"^" + re.escape(signature), clean, re.M)
              if clean[m.end():].lstrip().startswith("{")]
    if len(starts) != 1:
        raise LookupError("expected one definition of %r, found %d" % (signature, len(starts)))
    start = starts[0]
    i = clean.index("{", start)
    depth = 0
    while True:
        if clean[i] == "{":
            depth += 1
        elif clean[i] == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
        i += 1


def function_text(text, signature):
    start, end = function_span(text, signature)
    return text[start:end]


DEFINITION = re.compile(r"^[A-Za-z_][^\n;{}=]*?\b(?:LL\w+::)?(~?\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?\{", re.M)
INLINE = re.compile(r"\b(\w+)\s*\([^()]*\)\s*(?:const\s*)?\{")
_definitions = {}


def enclosing_function(clean, position):
    """Name of the function whose body holds `position`: an inline definition on
    the same line (class bodies), else the last column-0 definition before it."""
    line_start = clean.rfind("\n", 0, position) + 1
    inline = [m for m in INLINE.finditer(clean, line_start, position) if m.group(1) not in ("if", "for", "while", "switch")]
    if inline and clean[line_start:line_start + 1].isspace():
        return inline[0].group(1)
    if id(clean) not in _definitions:
        _definitions[id(clean)] = (clean, [(m.start(), m.group(1)) for m in DEFINITION.finditer(clean)])
    name = None
    for start, found in _definitions[id(clean)][1]:
        if start > position:
            break
        name = found
    return name


CLASS_DECL = re.compile(r"\bclass\s+(?:alignas\s*\(\s*\d+\s*\)\s*)?(?:LL_\w+\s+)?(\w+)\s*(?:final\s*)?:([^{;]+)\{")
QUALIFIED = re.compile(r"^[A-Za-z_][^\n;{}=]*?\b(\w+)::(~?\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:override\s*)?\{", re.M)
_joint_tree = {}


def joint_hierarchy():
    """(classes derived from LLAvatarJoint incl. itself, files that declare or define them). Cached."""
    if not _joint_tree:
        paths = [p for d in JOINT_DIRS for ext in ("*.h", "*.cpp", "*.inl") for p in sorted(d.glob(ext))]
        bases = {}
        mentions = {}
        for path in paths:
            text = read(path)
            if "Joint" not in text:
                continue
            mentions[path] = text
            if path.suffix == ".h":
                for match in CLASS_DECL.finditer(strip_comments(text)):
                    names = set(re.findall(r"\w+", match.group(2))) - {"public", "protected", "private", "virtual"}
                    bases.setdefault(match.group(1), set()).update(names)
        classes = {"LLAvatarJoint"}
        grew = True
        while grew:
            grew = False
            for name, parents in bases.items():
                if name not in classes and parents & classes:
                    classes.add(name)
                    grew = True
        names = "|".join(sorted(classes))
        member = re.compile(r"\b(?:%s)::" % names)
        declared = re.compile(r"\bclass\s+(?:alignas\s*\(\s*\d+\s*\)\s*)?(?:LL_\w+\s+)?(?:%s)\b[^;{]*\{" % names)
        files = [path for path, text in mentions.items()
                 if member.search(text) or (path.suffix == ".h" and declared.search(text))]
        _joint_tree["classes"], _joint_tree["files"], _joint_tree["declared"] = classes, files, declared
    return _joint_tree["classes"], _joint_tree["files"], _joint_tree["declared"]


def joint_flag_sites(src):
    """{(file name, Class::function or <header>): count} of mVisible in the joint hierarchy."""
    classes, files, declared = joint_hierarchy()
    by_path = {path: key for key, path in SOURCES.items()}
    found = {}
    for path in files:
        text = src[by_path[path]] if path in by_path else read(path)
        if "mVisible" not in text:
            continue
        clean = strip_comments(text)
        declares = path.suffix == ".h" and declared.search(clean) is not None
        definitions = [(m.start(), m.group(1), m.group(2)) for m in QUALIFIED.finditer(clean)]
        for match in re.finditer(r"\bmVisible\b", clean):
            owner = None
            for start, cls, name in definitions:
                if start > match.start():
                    break
                owner = (cls, name)
            if owner and owner[0] in classes:
                key = (path.name, "%s::%s" % owner)
            elif declares:
                key = (path.name, "<header>")
            else:
                continue  # another class's own mVisible (for example LLVOAvatar::mVisible)
            found[key] = found.get(key, 0) + 1
    return found


def brace_depth(clean, position):
    return clean.count("{", 0, position) - clean.count("}", 0, position)


def call_sites(text, pattern):
    """{enclosing function: count} for calls matching `pattern` (definitions excluded)."""
    clean = strip_comments(text)
    found = {}
    for match in re.finditer(pattern, clean):
        line_start = clean.rfind("\n", 0, match.start()) + 1
        before = clean[line_start:match.start()]
        if re.match(r"^[A-Za-z_]", before) and "::" in before and not before.rstrip().endswith(("->", ".")):
            continue  # an out-of-line definition
        if re.match(r"^\s*(static\s+)?(virtual\s+)?(void|S32|bool)\s+$", before):
            continue  # a declaration
        name = enclosing_function(clean, match.start())
        found[name] = found.get(name, 0) + 1
    return found


def source_findings(src):
    """Source pins. `src` maps the SOURCES keys to file text."""
    findings = []

    def need(condition, message):
        if not condition:
            findings.append(message)

    avatar, header, obj = src["avatar_cpp"], src["avatar_h"], src["object_cpp"]

    # -- callers ------------------------------------------------------------
    actual = {}
    for key in ("avatar_cpp", "avatar_h", "object_cpp", "panel_cpp"):
        for name, count in call_sites(src[key], r"\bupdateMeshVisibility\s*\(\s*\)").items():
            actual[(key, name)] = count
    need(actual == WALK_CALLERS,
         "direct updateMeshVisibility callers changed (audit, then list): expected %s, found %s"
         % (sorted(WALK_CALLERS.items()), sorted(actual.items())))
    actual = {}
    for key in ("avatar_cpp", "avatar_h", "object_cpp", "panel_cpp"):
        for name, count in call_sites(src[key], r"\brequestMeshVisibilityUpdate\s*\(\s*\)").items():
            actual[(key, name)] = count
    need(actual == REQUEST_CALLERS, "coalesced request callers changed: %s" % sorted(actual.items()))
    actual = {("object_cpp", name): count
              for name, count in call_sites(obj, r"\bupdateAvatarMeshVisibility\s*\(").items()}
    need(actual == CALLBACK_CALLERS, "updateAvatarMeshVisibility callers changed: %s" % sorted(actual.items()))
    try:
        callback = strip_comments(function_text(
            obj, "void LLViewerObject::updateAvatarMeshVisibility(const LLUUID& id, const LLUUID& old_id)"))
        need("avatar->requestMeshVisibilityUpdate();" in callback and "updateMeshVisibility(" not in callback,
             "the texture-entry callback must ask through requestMeshVisibilityUpdate")
        need("id == old_id" in callback and callback.count("isBakedImageId") == 2,
             "the texture-entry callback lost its unchanged / non-bake early returns")
    except LookupError as error:
        findings.append(str(error))

    # other files must not call either entry point
    for path in sorted(NEWVIEW.glob("*.cpp")) + sorted(NEWVIEW.glob("*.h")) + sorted(NEWVIEW.glob("*.inl")):
        if path in (SOURCES["avatar_cpp"], SOURCES["avatar_h"], SOURCES["object_cpp"], SOURCES["panel_cpp"],
                    SOURCES["display_cpp"]):
            continue
        text = read(path) if src.get("_scan_tree", True) else ""
        text = strip_comments(text) if "MeshVisibility" in text else ""
        need(not re.search(r"\b(updateMeshVisibility|requestMeshVisibilityUpdate|updateAvatarMeshVisibility"
                           r"|flushPendingMeshVisibility)\s*\(", text.replace("void updateAvatarMeshVisibility(", "")),
             "unaudited body-visibility call in %s" % path.name)

    # -- the walk and the mark ------------------------------------------------
    bodies = {}
    for name, signature in (("walk", "void LLVOAvatar::updateMeshVisibility()"),
                            ("clear", "void LLVOAvatar::clearMeshVisibilityDirty()"),
                            ("request", "void LLVOAvatar::requestMeshVisibilityUpdate()"),
                            ("flush", "void LLVOAvatar::flushPendingMeshVisibility()"),
                            ("markDead", "void LLVOAvatar::markDead()"),
                            ("destructor", "LLVOAvatar::~LLVOAvatar()")):
        try:
            bodies[name] = strip_comments(function_text(avatar, signature))
        except LookupError as error:
            findings.append(str(error))
            bodies[name] = ""
    walk_first = bodies["walk"][bodies["walk"].find("{") + 1:].lstrip()
    need(walk_first.startswith("clearMeshVisibilityDirty();"),
         "updateMeshVisibility must clear the mark first (every walk satisfies a pending request)")
    need(re.search(r"static\s+LLCachedControl<bool>\s+\w+\(gSavedSettings,\s*\"\s*\",\s*true\);", bodies["request"])
         is not None and ('"%s"' % SETTING) in avatar,
         "requestMeshVisibilityUpdate must read %s through a static LLCachedControl" % SETTING)
    need(re.search(r"if\s*\(\s*!\w+\s*\|\|\s*isDead\(\)\s*\)\s*\{\s*updateMeshVisibility\(\);\s*return;", bodies["request"])
         is not None, "switch off (or a dead avatar) must walk immediately")
    flush_first = bodies["flush"][bodies["flush"].find("{") + 1:].lstrip()
    need(re.match(r"if\s*\(\s*sMeshVisibilityDirtyCount\s*==\s*0\s*\)\s*\{\s*return;", flush_first) is not None,
         "the frame flush must return on one integer compare when nothing is marked")
    need("LLCachedControl" not in bodies["flush"] and "gSavedSettings" not in bodies["flush"],
         "the frame flush must not depend on the switch (it drains marks left when the switch goes off)")
    need(re.search(r"if\s*\(\s*sMeshVisibilityDirtyCount\s*!=\s*0\s*\)\s*\{\s*LL_WARNS_ONCE\(", bodies["flush"])
         is not None and "sMeshVisibilityDirtyCount = 0;" in bodies["flush"],
         "a dirty-count imbalance must warn once and reset")
    need("static_cast<LLVOAvatar*>(character)" in bodies["flush"], "the flush loop must use static_cast")
    for name in ("clear", "request", "flush"):
        for token in ALLOCATING:
            need(token not in bodies[name], "%s: allocation-like token %r" % (name, token.strip()))
    need("clearMeshVisibilityDirty();" in bodies["destructor"], "~LLVOAvatar must clear the mark")
    dead = bodies["markDead"]
    need("clearMeshVisibilityDirty();" in dead and "LLViewerObject::markDead();" in dead
         and dead.find("clearMeshVisibilityDirty();") < dead.find("LLViewerObject::markDead();"),
         "markDead must clear the mark before the object is marked dead")

    # -- readers and other writers flush first -----------------------------------
    signatures = {"renderSkinned": "U32 LLVOAvatar::renderSkinned()",
                  "renderTransparent": "U32 LLVOAvatar::renderTransparent(bool first_pass)",
                  "renderRigid": "U32 LLVOAvatar::renderRigid()",
                  "isActorGhostSystemReplayReady": "bool LLVOAvatar::isActorGhostSystemReplayReady()",
                  "hideHair": "void LLVOAvatar::hideHair()",
                  "hideSkirt": "void LLVOAvatar::hideSkirt()"}
    for name in GUARDED:
        try:
            body = strip_comments(function_text(avatar, signatures[name]))
        except LookupError as error:
            findings.append(str(error))
            continue
        guard = body.find("flushMeshVisibilityIfDirty();")
        uses = [body.find(token) for token in ("mMeshLOD", "getViewerJoint(", "return", "mesh_ready")]
        uses = [u for u in uses if u >= 0]
        need(guard >= 0 and (not uses or guard < min(uses)),
             "%s must flush a pending walk before it touches the body meshes" % name)
    clean_avatar = strip_comments(avatar)
    users = set(call_sites(avatar, r"\bgetViewerJoint\s*\(").keys())
    need(users <= JOINT_USERS, "new getViewerJoint user(s) %s: guard or justify" % sorted(users - JOINT_USERS))
    writers = {}
    for match in re.finditer(r"mMeshLOD\[[^\]]+\]->setVisible\(|\bjoint->setVisible\(", clean_avatar):
        name = enclosing_function(clean_avatar, match.start())
        writers[name] = writers.get(name, 0) + 1
    need(writers == {"hideHair": 1, "hideSkirt": 1, "updateMeshVisibility": 8},
         "body-part visibility writers changed: %s" % sorted(writers.items()))
    header_clean = strip_comments(header)
    need(re.search(r"void\s+flushMeshVisibilityIfDirty\(\)\s*\{\s*if\s*\(mMeshVisibilityDirty\)\s*"
                   r"\{\s*updateMeshVisibility\(\);\s*\}\s*\}", header_clean) is not None,
         "flushMeshVisibilityIfDirty changed")
    need(re.search(r"bool\s+mMeshVisibilityDirty\s*=\s*false;", header_clean) is not None
         and re.search(r"static\s+S32\s+sMeshVisibilityDirtyCount;", header_clean) is not None,
         "the dirty mark / count members changed")
    sites = joint_flag_sites(src)
    need(sites == JOINT_FLAG_SITES,
         "readers / writers of the joint visibility flag changed anywhere in the joint hierarchy "
         "(guard the new reader, then list it): unexpected %s, missing %s"
         % (sorted(set(sites.items()) - set(JOINT_FLAG_SITES.items())),
            sorted(set(JOINT_FLAG_SITES.items()) - set(sites.items()))))
    joint_header = strip_comments(src["avatarjoint_h"])
    need(re.search(r"protected:(?:(?!public:|private:).)*?\bbool\s+mVisible;", joint_header, re.S) is not None
         and "friend" not in joint_header,
         "LLAvatarJoint::mVisible must stay protected with no friend (readers stay inside the hierarchy)")

    # -- frame order ---------------------------------------------------------------
    display = strip_comments(src["display_cpp"])
    need(display.count("LLVOAvatar::flushPendingMeshVisibility();") == 1, "expected exactly one frame flush call")
    try:
        start, end = function_span(src["display_cpp"],
                                   "void display(bool rebuild, F32 zoom_factor, int subfield, bool for_snapshot)")
        body = display[start:end]
        flush = body.find("LLVOAvatar::flushPendingMeshVisibility();")
        images = body.rfind("gTextureList.updateImages(")
        materials = body.find("gGLTFMaterialList.flushMaterials();")
        prism = body.find("LLPrismLens::renderAuxiliaryView();")
        sort = body.find("gPipeline.stateSort(")
        geom = max(body.find("gPipeline.renderGeomDeferred("), body.find("gPipeline.renderGeom("),
                   body.find("render_ui("))
        need(flush >= 0 and 0 <= images < flush and 0 <= materials < flush,
             "the frame flush must come after the image and material updates (their callbacks can ask)")
        need(flush >= 0 and flush < prism and flush < sort and (geom < 0 or flush < geom) and prism >= 0 and sort >= 0,
             "the frame flush must come before the Prism view, the state sort and the geometry passes")
        # Unconditional on every frame that renders the world: directly inside the one
        # "if (!gDisconnected && !LLApp::isExiting())" block that also holds the Prism
        # view, the state sort and the geometry passes, under no other condition.
        opener = -1
        if flush >= 0:
            depth = 0
            for index in range(flush - 1, -1, -1):
                if body[index] == "}":
                    depth += 1
                elif body[index] == "{":
                    if depth == 0:
                        opener = index
                        break
                    depth -= 1
        need(flush >= 0 and brace_depth(body, flush) == 2 and brace_depth(body, prism) == 2 and opener >= 0
             and body[:opener].rstrip().endswith("if (!gDisconnected && !LLApp::isExiting())"),
             "the frame flush must sit on the unconditional per-frame render path of display()")
    except LookupError as error:
        findings.append(str(error))

    # -- setting -------------------------------------------------------------------
    block = re.search(r"<key>%s</key>\s*<map>(.*?)</map>" % SETTING, src["settings"], re.S)
    need(src["settings"].count("<key>%s</key>" % SETTING) == 1 and block is not None, "setting missing or duplicated")
    if block:
        text = block.group(1)
        need("<key>Persist</key><integer>1</integer>" in text, "setting must persist")
        need("<key>Type</key><string>Boolean</string>" in text, "setting must be a Boolean")
        need("<key>Value</key><integer>1</integer>" in text, "setting must default to TRUE")
        need("FALSE" in text and "previous behaviour" in text, "setting comment must say what FALSE does")
    return findings


# ---------------------------------------------------------------------------
# Native fixture
# ---------------------------------------------------------------------------
T = {"EQUIVALENCE": 1, "COALESCED": 2, "IMMEDIATE": 4, "SWITCH_OFF": 8, "LIVE_TOGGLE": 16, "DELETE": 32,
     "CLEAN": 64, "ORDER": 128, "ASSERT": 256}


def replace_once(text, old, new):
    if text.count(old) < 1:
        raise RuntimeError("planted negative no longer applies: %r" % old)
    return text.replace(old, new, 1)


def production_pieces(src):
    avatar, header, obj = src["avatar_cpp"], src["avatar_h"], src["object_cpp"]
    start = header.index("    void            requestMeshVisibilityUpdate();")
    end = header.index("    static S32      sMeshVisibilityDirtyCount;") + len("    static S32      sMeshVisibilityDirtyCount;")
    if "clearMeshVisibilityDirty();" not in function_text(avatar, "void LLVOAvatar::markDead()") \
            or "clearMeshVisibilityDirty();" not in function_text(avatar, "LLVOAvatar::~LLVOAvatar()"):
        raise RuntimeError("markDead / destructor no longer clear the mark")
    methods = ["S32 LLVOAvatar::sMeshVisibilityDirtyCount = 0;"]
    for signature in ("void LLVOAvatar::clearMeshVisibilityDirty()",
                      "void LLVOAvatar::requestMeshVisibilityUpdate()",
                      "void LLVOAvatar::flushPendingMeshVisibility()",
                      "void LLVOAvatar::updateMeshVisibility()",
                      "void LLVOAvatar::hideHair()",
                      "void LLVOAvatar::hideSkirt()"):
        methods.append(function_text(avatar, signature))
    methods.append(function_text(
        obj, "void LLViewerObject::updateAvatarMeshVisibility(const LLUUID& id, const LLUUID& old_id)"))
    return {"header": header[start:end], "methods": "\n\n".join(methods),
            "dead": "clearMeshVisibilityDirty();", "destructor": "clearMeshVisibilityDirty();", "guarded": "true"}


# name -> (mutation of the pieces, tests that must fail)
def planted_variants():
    def methods(old, new):
        return lambda p: dict(p, methods=replace_once(p["methods"], old, new))

    return [
        ("real", lambda p: p, 0),
        ("walk_keeps_mark",
         methods("    // Every walk, immediate or coalesced, satisfies a pending request.\n    clearMeshVisibilityDirty();\n", ""),
         T["IMMEDIATE"]),
        ("request_ignores_switch", methods("if (!coalesce || isDead())", "if (isDead())"),
         T["SWITCH_OFF"] | T["LIVE_TOGGLE"]),
        ("request_never_coalesces", methods("if (!coalesce || isDead())", "if (true)"), T["COALESCED"]),
        ("request_marks_dead_avatar", methods("if (!coalesce || isDead())", "if (!coalesce)"), T["DELETE"]),
        ("callback_walks_directly",
         methods("avatar->requestMeshVisibilityUpdate();", "avatar->updateMeshVisibility();"), T["COALESCED"]),
        ("hide_skirt_unguarded",
         methods("    flushMeshVisibilityIfDirty();\n    mMeshLOD[MESH_ID_SKIRT]", "    mMeshLOD[MESH_ID_SKIRT]"),
         T["ORDER"] | T["EQUIVALENCE"]),
        ("hide_hair_unguarded",
         methods("    flushMeshVisibilityIfDirty();\n    mMeshLOD[MESH_ID_HAIR]", "    mMeshLOD[MESH_ID_HAIR]"),
         T["ORDER"] | T["EQUIVALENCE"]),
        ("clear_keeps_count", methods("        --sMeshVisibilityDirtyCount;\n", ""), T["DELETE"] | T["IMMEDIATE"]),
        ("mark_twice_counts_twice", methods("    if (!mMeshVisibilityDirty)\n    {\n        mMeshVisibilityDirty = true;",
                                            "    if (true)\n    {\n        mMeshVisibilityDirty = true;"), T["COALESCED"]),
        ("flush_without_empty_check",
         methods("    if (sMeshVisibilityDirtyCount == 0)\n    {\n        return;\n    }", ""), T["CLEAN"]),
        ("flush_skips_walk", methods("            avatar->updateMeshVisibility();\n", ""), T["EQUIVALENCE"] | T["COALESCED"]),
        ("flush_respects_switch_off",
         methods("    for (LLCharacter* character : LLCharacter::sInstances)",
                 "    if (!gSwitch) return;\n    for (LLCharacter* character : LLCharacter::sInstances)"),
         T["LIVE_TOGGLE"]),
        ("reader_unguarded", lambda p: dict(p, guarded="false"), T["ORDER"] | T["EQUIVALENCE"]),
        ("markdead_keeps_mark", lambda p: dict(p, dead=""), T["DELETE"]),
        ("destructor_keeps_mark", lambda p: dict(p, destructor=""), T["DELETE"]),
    ]


def build_fixture(src):
    template = read(Path(__file__).with_name("body_visibility_coalesce_fixture.cpp"))
    head, rest = template.split("// VARIANT_BEGIN\n", 1)
    block, tail = rest.split("// VARIANT_END\n", 1)
    pieces = production_pieces(src)
    variants = planted_variants()
    blocks, calls = [], []
    for name, mutate, _ in variants:
        p = mutate(dict(pieces))
        text = block.replace("VARIANT_NAME", "variant_" + name)
        for marker, value in (("// PRODUCTION_HEADER", p["header"]), ("// PRODUCTION_METHODS", p["methods"]),
                              ("// PRODUCTION_MARKDEAD_CLEAR", p["dead"]),
                              ("// PRODUCTION_DESTRUCTOR_CLEAR", p["destructor"])):
            if text.count(marker) != 1:
                raise RuntimeError("fixture marker %s missing" % marker)
            text = text.replace(marker, value)
        text = replace_once(text, "bool gReadersGuarded = true;", "bool gReadersGuarded = %s;" % p["guarded"])
        blocks.append(text)
        calls.append('    std::printf("VARIANT %s %%d\\n", variant_%s::run());' % (name, name))
    source = head + "\n".join(blocks) + tail.replace("    // VARIANT_CALLS", "\n".join(calls))
    return source, variants


def fixture_findings(src):
    findings = []
    source, variants = build_fixture(src)
    directory = native.check_directory("body_visibility_coalesce")
    result = native.compile_run(directory, source, name="body_visibility_coalesce", flags=("/W3", "/Od"))
    if result.returncode:
        return ["fixture exited %d: %s%s" % (result.returncode, result.stdout, result.stderr)]
    masks = dict(re.findall(r"^VARIANT (\w+) (\d+)$", result.stdout, re.M))
    names = {v: k for k, v in T.items()}

    def describe(mask):
        return "+".join(names[bit] for bit in sorted(names) if mask & bit) or "none"

    for name, _, expected in variants:
        if name not in masks:
            findings.append("fixture variant %s did not report" % name)
            continue
        mask = int(masks[name])
        if name == "real":
            if mask:
                findings.append("production code fails fixture tests: %s" % describe(mask))
        elif mask & expected != expected:
            findings.append("planted negative %s was not caught by %s (failed: %s)"
                            % (name, describe(expected), describe(mask)))
        print("  fixture %-28s failed tests: %s" % (name, describe(mask)))
    return findings


def source_negatives(src):
    """Each planted source change must produce a finding containing the given text."""
    def edit(key, old, new):
        return lambda s: dict(s, **{key: replace_once(s[key], old, new)})

    cases = [
        ("render_skinned_unguarded",
         edit("avatar_cpp", "    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;\n    flushMeshVisibilityIfDirty();\n",
              "    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;\n"), "renderSkinned must flush"),
        ("render_rigid_unguarded",
         edit("avatar_cpp", "U32 LLVOAvatar::renderRigid()\n{\n    flushMeshVisibilityIfDirty();\n",
              "U32 LLVOAvatar::renderRigid()\n{\n"), "renderRigid must flush"),
        ("render_transparent_unguarded",
         edit("avatar_cpp", "U32 LLVOAvatar::renderTransparent(bool first_pass)\n{\n    flushMeshVisibilityIfDirty();\n",
              "U32 LLVOAvatar::renderTransparent(bool first_pass)\n{\n"), "renderTransparent must flush"),
        ("replay_ready_unguarded",
         edit("avatar_cpp", "bool LLVOAvatar::isActorGhostSystemReplayReady()\n{\n    flushMeshVisibilityIfDirty();\n",
              "bool LLVOAvatar::isActorGhostSystemReplayReady()\n{\n"), "isActorGhostSystemReplayReady must flush"),
        ("attach_coalesced",
         edit("avatar_cpp", "    updateMeshVisibility();\n\n    ALAvatarAnimSpeed::instance().applyAttachment(*this, *viewer_object);",
              "    requestMeshVisibilityUpdate();\n\n    ALAvatarAnimSpeed::instance().applyAttachment(*this, *viewer_object);"),
         "direct updateMeshVisibility callers changed"),
        ("appearance_coalesced",
         edit("avatar_cpp", "    updateMeshTextures();\n    updateMeshVisibility();\n\n}",
              "    updateMeshTextures();\n    requestMeshVisibilityUpdate();\n\n}"),
         "direct updateMeshVisibility callers changed"),
        ("overall_appearance_coalesced",
         edit("avatar_cpp", "            mLastImpostorUpdateReason = 8;\n        }\n        updateMeshVisibility();",
              "            mLastImpostorUpdateReason = 8;\n        }\n        requestMeshVisibilityUpdate();"),
         "coalesced request callers changed"),
        ("new_unlisted_walk_caller",
         edit("avatar_cpp", "void LLVOAvatar::dirtyMesh()\n{", "void LLVOAvatar::dirtyMesh()\n{\n    updateMeshVisibility();"),
         "direct updateMeshVisibility callers changed"),
        ("new_writer",
         edit("avatar_cpp", "void LLVOAvatar::dirtyMesh()\n{",
              "void LLVOAvatar::dirtyMesh()\n{\n    mMeshLOD[MESH_ID_HEAD]->setVisible(false, true);"),
         "visibility writers changed"),
        ("new_reader",
         edit("jointmesh_cpp", "bool LLViewerJointMesh::isActorGhostPaletteReady() const\n{",
              "bool LLViewerJointMesh::isActorGhostPaletteReady() const\n{\n    if (!mVisible) return false;"),
         "joint visibility flag changed anywhere"),
        ("joint_render_reads_flag",
         edit("joint_cpp", "    U32 triangle_count = 0;\n", "    U32 triangle_count = 0;\n    if (!mVisible) return 0;\n"),
         "joint visibility flag changed anywhere"),
        ("joint_accessor_added",
         edit("avatarjoint_h", "    void setVisible( bool visible, bool recursive );",
              "    void setVisible( bool visible, bool recursive );\n    bool getVisible() const { return mVisible; }"),
         "joint visibility flag changed anywhere"),
        ("joint_flag_made_public",
         edit("avatarjoint_h", "    LLJointPickName mPickName;\n    bool        mVisible;",
              "    LLJointPickName mPickName;\npublic:\n    bool        mVisible;"),
         "must stay protected"),
        ("frame_flush_conditional",
         edit("display_cpp", "        LLVOAvatar::flushPendingMeshVisibility();\n",
              "        if (rebuild)\n        {\n            LLVOAvatar::flushPendingMeshVisibility();\n        }\n"),
         "unconditional per-frame render path"),
        ("flush_c_cast",
         edit("avatar_cpp", "LLVOAvatar* avatar = static_cast<LLVOAvatar*>(character);\n        if (avatar->mMeshVisibilityDirty)",
              "LLVOAvatar* avatar = (LLVOAvatar*)character;\n        if (avatar->mMeshVisibilityDirty)"),
         "must use static_cast"),
        ("imbalance_silent",
         edit("avatar_cpp", "        LL_WARNS_ONCE(\"Avatar\") << \"Body visibility dirty count out of balance: \"\n",
              "        LL_DEBUGS(\"Avatar\") << \"x\"\n"),
         "must warn once"),
        ("markdead_keeps_mark",
         edit("avatar_cpp", "    // A dead avatar never keeps a pending body-visibility walk.\n    clearMeshVisibilityDirty();\n", ""),
         "markDead must clear"),
        ("markdead_clears_late",
         lambda s: dict(s, avatar_cpp=replace_once(replace_once(
             s["avatar_cpp"], "    // A dead avatar never keeps a pending body-visibility walk.\n    clearMeshVisibilityDirty();\n", ""),
             "    LLViewerObject::markDead();\n}", "    LLViewerObject::markDead();\n    clearMeshVisibilityDirty();\n}")),
         "markDead must clear"),
        ("destructor_keeps_mark",
         edit("avatar_cpp", "    clearMeshVisibilityDirty();\n    sInstances.remove(this);", "    sInstances.remove(this);"),
         "~LLVOAvatar must clear"),
        ("frame_flush_missing",
         edit("display_cpp", "        LLVOAvatar::flushPendingMeshVisibility();\n", ""), "exactly one frame flush"),
        ("frame_flush_after_state_sort",
         lambda s: dict(s, display_cpp=replace_once(replace_once(
             s["display_cpp"], "        LLVOAvatar::flushPendingMeshVisibility();\n", ""),
             "            gPipeline.stateSort(*LLViewerCamera::getInstance(), result);\n",
             "            gPipeline.stateSort(*LLViewerCamera::getInstance(), result);\n"
             "            LLVOAvatar::flushPendingMeshVisibility();\n")),
         "before the Prism view"),
        ("frame_flush_before_images",
         lambda s: dict(s, display_cpp=replace_once(replace_once(
             s["display_cpp"], "        LLVOAvatar::flushPendingMeshVisibility();\n", ""),
             "        LLAppViewer::instance()->pingMainloopTimeout(\"Display:UpdateImages\");\n",
             "        LLVOAvatar::flushPendingMeshVisibility();\n"
             "        LLAppViewer::instance()->pingMainloopTimeout(\"Display:UpdateImages\");\n")),
         "after the image and material updates"),
        ("flush_reads_setting",
         edit("avatar_cpp", "    // Runs whatever the switch says, so marks left from before it was turned\n",
              "    if (!gSavedSettings.getBOOL(\"x\")) return;\n    // Runs whatever the switch says, so marks left from before it was turned\n"),
         "must not depend on the switch"),
        ("request_allocates",
         edit("avatar_cpp", "    if (!mMeshVisibilityDirty)\n    {\n        mMeshVisibilityDirty = true;",
              "    if (!mMeshVisibilityDirty)\n    {\n        sPending.push_back(this);\n        mMeshVisibilityDirty = true;"),
         "allocation-like token"),
        ("setting_default_off",
         edit("settings", "previous behaviour (a full check on every change). Takes effect immediately.</string>\n"
              "        <key>Persist</key><integer>1</integer>\n        <key>Type</key><string>Boolean</string>\n"
              "        <key>Value</key><integer>1</integer>",
              "previous behaviour (a full check on every change). Takes effect immediately.</string>\n"
              "        <key>Persist</key><integer>1</integer>\n        <key>Type</key><string>Boolean</string>\n"
              "        <key>Value</key><integer>0</integer>"),
         "default to TRUE"),
        ("setting_not_persisted",
         edit("settings", "Takes effect immediately.</string>\n        <key>Persist</key><integer>1</integer>",
              "Takes effect immediately.</string>\n        <key>Persist</key><integer>0</integer>"),
         "must persist"),
        ("setting_missing", edit("settings", "<key>%s</key>" % SETTING, "<key>Renamed</key>"), "setting missing"),
    ]
    findings = []
    for name, mutate, expected in cases:
        try:
            mutated = mutate(dict(src))
        except RuntimeError as error:
            findings.append("source negative %s: %s" % (name, error))
            continue
        mutated["_scan_tree"] = False
        got = source_findings(mutated)
        if not any(expected in finding for finding in got):
            findings.append("source negative %s was not caught (wanted %r, got %s)" % (name, expected, got))
        else:
            print("  source  %-28s caught" % name)
    return findings


_fixture_run = {}


def load_sources():
    missing = [str(path) for path in SOURCES.values() if not path.is_file()]
    if missing:
        print("missing input: " + ", ".join(missing))
        return None
    return {key: read(path) for key, path in SOURCES.items()}


def fixture_once(src):
    """One compile serves the check (the real variant) and the selftest (the planted ones)."""
    if "findings" not in _fixture_run:
        try:
            _fixture_run["findings"] = fixture_findings(src)
        except (RuntimeError, LookupError, ValueError, OSError) as error:
            _fixture_run["findings"] = ["fixture: %s" % error]
    return _fixture_run["findings"]


def report(findings, label):
    for finding in findings:
        print("FINDING: " + finding)
    print("body_visibility_coalesce_check %s: %s" % (label, "FAIL (%d)" % len(findings) if findings else "PASS"))
    return 1 if findings else 0


def run_check():
    """Source pins on the production tree, and the production methods through the fixture."""
    src = load_sources()
    if src is None:
        return 2
    findings = source_findings(src)
    print("source pins: %d finding(s)" % len(findings))
    findings += [f for f in fixture_once(src) if not f.startswith("planted negative")]
    return report(findings, "check")


def selftest():
    """Every planted negative, source and native, must be caught."""
    src = load_sources()
    if src is None:
        return 2
    findings = source_negatives(src)
    findings += [f for f in fixture_once(src) if f.startswith(("planted negative", "fixture"))]
    return report(findings, "selftest")


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--check", action="store_true", help="source pins + production methods in the fixture")
    parser.add_argument("--selftest", action="store_true", help="planted negatives must be caught")
    args = parser.parse_args(argv)
    both = not args.check and not args.selftest
    status = 0
    if args.check or both:
        status = run_check() or status
    if args.selftest or both:
        status = selftest() or status
    return status


if __name__ == "__main__":
    sys.exit(main())
