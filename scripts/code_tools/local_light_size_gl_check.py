#!/usr/bin/env python3
"""Production GLSL caller fixture; hidden WGL, no viewer build or launch.

python scripts/code_tools/local_light_size_gl_check.py --assemble-only
python scripts/code_tools/local_light_size_gl_check.py --gpu --cost

Full deferred main bodies and forward evaluators run with analytic plane inputs.
Uses the existing production linker and caller-fixture builder, not copied BRDFs.
HEAD and WORKTREE share one GL context; zero-radius images must match bit for bit.
The cost gate alternates size 0/.15 at 3840x2160 with 32 ordinary lights, p95 <= .1 ms.
GPU runs require tasklist to establish closure, or explicit user confirmation when process access is denied.
"""
import argparse
import ast
import ctypes as C
import hashlib
import json
import itertools
from pathlib import Path
import re
import subprocess
import sys
import time

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SHADERS = ROOT / "indra/newview/app_settings/shaders"
PATHS = ("point", "multiPoint", "spot", "multiSpot", "actorPoint", "actorSpot",
         "alpha", "material", "pbralpha", "pbrsplit", "actorPbr")


def production_permutations(path):
    """LLViewerShaderMgr loadShadersDeferred plus runtime settings/fallbacks.

    Presence macros are omitted when false (a defined zero is still #ifdef true).
    Include the non-actor material fallbacks as adversarial coverage as well.
    Weather is attached only to world, non-velocity, non-skinned programs.
    """
    name = Path(path).name
    common = [dict(**({"HAS_EMISSIVE": 1} if emissive else {}),
                   **({"EMISSIVE_SILHOUETTE_ONLY": 1} if silhouette else {}),
                   **({"HAS_VISIBLE_DIFFUSE": 1} if visible else {}))
              for emissive, silhouette in ((0, 0), (1, 0), (1, 1)) for visible in (0, 1)]
    bases = []
    if name == "materialF.glsl":
        for mode, normal, spec, skin, shadow, actor in itertools.product(range(4), *(range(2),)*5):
            base = {"DIFFUSE_ALPHA_MODE": mode}
            for flag, enabled in (("HAS_NORMAL_MAP", normal), ("HAS_SPECULAR_MAP", spec),
                                  ("HAS_SKIN", skin), ("HAS_SUN_SHADOW", shadow),
                                  ("HAS_ACTOR_FX", actor), ("HAS_ALPHA_MASK", mode != 0)):
                if enabled:
                    base[flag] = 1
            bases.append(base)
            if mode == 1:
                bases.append(dict(base, MATERIAL_VELOCITY=1))
    elif name in ("pbralphaF.glsl", "sharedActorFxPbrF.glsl"):
        maps = dict(HAS_ACTOR_FX=1, HAS_NORMAL_MAP=1, HAS_SPECULAR_MAP=1,
                    HAS_EMISSIVE_MAP=1, USE_VERTEX_COLOR=1)
        for skin, shadow, parity in itertools.product(range(2), repeat=3):
            base = dict(maps)
            for key, enabled in (("HAS_SKIN", skin), ("HAS_SUN_SHADOW", shadow),
                                 ("ALR_RIMSPLIT_PARITY", parity)):
                if enabled:
                    base[key] = 1
            if name == "pbralphaF.glsl":
                bases.append(dict(base, DIFFUSE_ALPHA_MODE=1))
            else:
                for mode, alpha in ((0, 0), (1, 2), (2, 1)):
                    replay = dict(base, SHARED_ACTOR_FX_REPLAY=1,
                                  SHARED_ACTOR_FX_ALPHA_MODE=mode, DIFFUSE_ALPHA_MODE=alpha)
                    if mode == 1:
                        replay["HAS_ALPHA_MASK"] = 1
                    bases.append(replay)
                    for channels in range(1, 9):
                        bases.append(dict(replay, SHARED_ACTOR_FX_SLOT_FILTER=1,
                                          GLTF_INDEXED_CHANNELS=channels))
        if name == "pbralphaF.glsl":
            bases.append(dict(IS_HUD=1))
    elif name == "alphaF.glsl":
        for skin, shadow, indexed, mask in itertools.product(range(2), repeat=4):
            base = dict(HAS_ACTOR_FX=1, USE_VERTEX_COLOR=1)
            for key, enabled in (("HAS_SKIN", skin), ("HAS_SUN_SHADOW", shadow),
                                 ("USE_INDEXED_TEX", indexed), ("USE_DIFFUSE_TEX", not indexed),
                                 ("HAS_ALPHA_MASK", mask)):
                if enabled:
                    base[key] = 1
            bases.append(base)
        bases += [dict(IS_HUD=1, USE_INDEXED_TEX=1, USE_VERTEX_COLOR=1, HAS_ALPHA_MASK=1),
                  dict(IS_AVATAR_SKIN=1, HAS_ACTOR_FX=1, USE_DIFFUSE_TEX=1,
                       USE_VERTEX_COLOR=1, HAS_SUN_SHADOW=1)]
    else:
        counts = range(1, 17) if name == "multiPointLightF.glsl" else (None,)
        spots = (0, 1) if name == "spotLightF.glsl" else (0,)
        for count, spot, aux, actor in itertools.product(counts, spots, range(2), range(2)):
            base = {}
            if count:
                base["LIGHT_COUNT"] = count
            if spot:
                base["MULTI_SPOTLIGHT"] = 1
            if aux:
                base["RIG_RIM_AUX"] = 1
            if actor:
                base["main"] = "actorMaskedMain"
            bases.append(base)
    variants = set()
    for base in bases:
        for extra in ([{}] if name == "sharedActorFxPbrF.glsl" else common):
            flags = dict(base, **extra)
            weather = name in ("materialF.glsl", "pbralphaF.glsl", "sharedActorFxPbrF.glsl", "alphaF.glsl")
            weather = weather and not any(k in flags for k in ("HAS_SKIN", "IS_AVATAR_SKIN", "IS_HUD", "MATERIAL_VELOCITY"))
            for enabled in ((0, 1) if weather else (0,)):
                candidate = dict(flags)
                if enabled:
                    candidate["HAS_WEATHER_MATERIAL"] = 1
                for avatar_tag in (0, 1):
                    tagged = dict(candidate)
                    if avatar_tag:
                        tagged["GBUFFER_AVATAR_TAG"] = 1
                    variants.add("".join(f"#define {k} {v}\n" for k, v in sorted(tagged.items())))
    return sorted(variants)


def read(ref, path):
    path = Path(path)
    rel = path.relative_to(ROOT).as_posix() if path.is_absolute() else path.as_posix()
    if ref == "WORKTREE":
        return (ROOT / rel).read_text(encoding="utf-8")
    return subprocess.run(["git", "show", ref + ":" + rel], cwd=ROOT, timeout=20,
                          capture_output=True, text=True, check=True).stdout


def builder(ref):
    # Reuse the existing fixture's complete production caller bodies. Execute
    # only its input adapter construction, never its context or test top level.
    source = (HERE / "relight_rig_check.py").read_text(encoding="utf-8")
    tree = ast.parse(source)
    inputs = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                  and any(isinstance(t, ast.Name) and t.id == "rig_inputs" for t in n.targets))
    inputs = inputs.replace("const vec3 testPos=vec3(0,0,-4);", "vec3 testPos;")
    inputs += "\nuniform vec2 testResolution;\nuniform float testRadius;\n"
    loop = next(n for n in tree.body if isinstance(n, ast.For)
                and isinstance(n.target, ast.Name) and n.target.id == "rig_path")
    segment = ast.get_source_segment(source, loop)
    body = segment[segment.index("\n")+1:segment.index("    pr=link_production")]
    ns = dict(re=re, root=SHADERS, rig_inputs=inputs, read_shader=lambda p: read(ref, p))
    exec(compile("def make(rig_path):\n" + body + "    return file, defines, fragment\n",
                 "existing_production_fixture_builder", "exec"), ns)
    return ns["make"]


def fixture(ref, path):
    original_path = {"multiSpot": "spot", "actorPoint": "multiPoint", "actorSpot": "spot"}.get(path, path)
    file, defines, fragment = builder(ref)(original_path)
    # A fixed light illuminates a spatial plane; the existing one-pixel fixture
    # instead puts the emitter at a constant offset from each receiver.
    fragment = fragment.replace("testPos+testLight", "vec3(0,0,-4)+testLight")
    if ref == "WORKTREE" and original_path not in ("point", "multiPoint", "spot", "alpha"):
        fragment = fragment.replace("0,0.0,testShape", "0,testRadius,testShape")
    position = "testPos=vec3((gl_FragCoord.xy/testResolution-.5)*.5,-4);"
    fragment = re.sub(r"void main\(\)\s*\{", lambda m: m[0]+position, fragment)
    if path in ("multiSpot", "actorSpot"):
        defines += "#define MULTI_SPOTLIGHT 1\n"
    if path in ("actorPoint", "actorSpot"):
        defines += "#define main actorMaskedMain\n"
    assert position in fragment
    return file, defines, fragment


def shader_linker(gl, ref, compile_only=False):
    import relight_3d_gl as G
    source = (HERE / "relight_shader_check.py").read_text(encoding="utf-8")
    module = ast.parse(source)
    wanted = {"production_file", "link_production", "production_variants"}
    definitions = "\n\n".join(ast.get_source_segment(source, n) for n in module.body
                              if isinstance(n, ast.FunctionDef) and n.name in wanted)
    compiler = G.AreaRunner.__new__(G.AreaRunner)
    compiler.gl = gl
    compiled = {}
    source_cache, token_cache, source_hashes = {}, {}, {}

    def source(path):
        # One shader build uses one immutable source snapshot, like the viewer.
        key = str(path)
        if key not in source_cache:
            source_cache[key] = read(ref, path)
            source_hashes[key] = hashlib.sha256(source_cache[key].encode()).hexdigest()
        return source_cache[key]

    def compile_part(text, defines, label, stage=0x8B30):
        # Shader objects are compiled separately, just as in the viewer. A
        # program define absent from an object's token stream cannot affect
        # that object. Drop only those unused defines to share identical GL
        # objects across the exhaustive program matrix, keeping every active
        # preprocessor branch and the production source intact.
        if text not in token_cache:
            token_cache[text] = set(re.findall(r"\b[A-Za-z_]\w*\b", text))
        tokens = token_cache[text]
        active = []
        for line in defines.splitlines():
            match = re.match(r"#define\s+(\w+)", line)
            if match is None or match[1] in tokens:
                active.append(line)
        avatar_tag = "#define GBUFFER_AVATAR_TAG 1" in defines
        defines = "\n".join(active) + "\n"
        if avatar_tag:
            # LLShaderMgr::loadShaderFile emits this global preamble in EVERY
            # stage/object, including modules without direct tag conditionals.
            defines += """
#ifndef GBUFFER_AVATAR_TAG
#define GBUFFER_AVATAR_TAG 1
#endif
#define GBUFFER_UNTAG(d) ((floor((d)*6.0+0.5) + mod(floor((d)*6.0+0.5), 2.0)) / 6.0)
#define GBUFFER_AVATAR_TAG_OF(d) (mod(floor((d)*6.0+0.5), 2.0) > 0.5)
#undef GET_GBUFFER_FLAG
#define GET_GBUFFER_FLAG(data, flag) (abs(GBUFFER_UNTAG(data)-(flag)) < 0.1)
"""
        prepared = (defines + text if "/*[EXTRA_CODE_HERE]*/" not in text else
                    text.replace("/*[EXTRA_CODE_HERE]*/", "/*[EXTRA_CODE_HERE]*/\n"+defines, 1))
        key = (stage, prepared)
        if key not in compiled:
            compiled[key] = compiler._compile(prepared, label, stage)
        return compiled[key]

    def use(program):
        gl.use(program)
        for name, value in [(b"relight_highlight_scale", 1), (b"relight_legacy_exponent", 368),
                            (b"local_light_spec_exponent", 368)]:
            gl.uf[1](gl.loc(program, name), value)

    ns = dict(Path=Path, root=SHADERS, C=C, I=C.c_int, program=gl.program, attach=gl.attach,
              link=gl.link, getprog=gl.getprog, plog=gl.plog, loc=gl.loc, ui=gl.ui, use=use,
              read_shader=source,
              compile_part=compile_part)
    exec(compile(definitions, "existing_production_linker", "exec"), ns)
    trace = ROOT / ".tmp/local_light_size_gl/last_link.json"
    trace.parent.mkdir(parents=True, exist_ok=True)
    production_link = ns["link_production"]
    def traced_link(file, defines, label, test_fragment=None, defer_link=False):
        trace.write_text(
            json.dumps(dict(file=file, defines=defines, label=label, ref=ref), indent=2), encoding="utf-8")
        return production_link(file, defines, label, test_fragment, compile_only=compile_only, defer_link=defer_link)
    ns["link_production"] = traced_link
    ns["source_hashes"] = source_hashes
    return ns


class Runner:
    def __init__(self, gl, program, width=257, height=257):
        self.gl, self.program = gl, program
        self.width, self.height = width, height
        self.shape = 0.0
        self.textures = []
        gl.use(program)
        self.tex(0, width, height, None)
        self.fbo = C.c_uint()
        gl.gen_fbo(1, C.byref(self.fbo))
        gl.bind_fbo(0x8D40, self.fbo)
        gl.fbo_tex(0x8D40, 0x8CE0, 0x0DE1, self.textures[0], 0)
        assert gl.fbo_status(0x8D40) == 0x8CD5, "float framebuffer incomplete"
        self.tex(2, 1, 1, np.ones((1, 1, 4), np.float32))
        self.tex(3, width, height, np.ones((height, width, 4), np.float32))
        # Same R16F Blinn LUT construction and settings as the viewer.
        import xml.etree.ElementTree as ET
        entries = list(ET.fromstring(read("WORKTREE", "indra/newview/app_settings/settings.xml")).find("map"))
        def setting(key):
            fields = list(entries[next(i for i, n in enumerate(entries) if n.text == key)+1])
            return float(fields[next(i for i, n in enumerate(fields) if n.text == "Value")+1].text)
        w, h, exponent = int(setting("RenderSpecularResX")), int(setting("RenderSpecularResY")), setting("RenderSpecularExponent")
        x = np.linspace(0, 1, w, dtype=np.float32)[None, :]
        n = np.linspace(0, 1, h, dtype=np.float32)[:, None]**2*exponent
        lut = np.ascontiguousarray(x**n*((n+2)*(n+4)/(8*np.pi*(2**(-n/2)+n))))
        self.tex(1, w, h, lut, 0x822D, 0x1903)
        gl.active(0x84C0)
        for name, value in [("lightFunc", 1), ("projectionMap", 2), ("actorMaskMap", 3),
                            ("actor_mask_bits", 255), ("proj_shadow_idx", -1), ("gobo_pattern", -1)]:
            self.integer(name, value)
        for name, values in [("testResolution", (width, height)), ("testNormal", (0, 0, 1)),
                             ("testShape", (0, 0, 1, -1)), ("testRoughness", (.25,)),
                             ("testGlossiness", (.75,)), ("testFalloff", (.5,)),
                             ("testLightPower", (1,)), ("color", (1,1,1)), ("testSilKeep", (1,)),
                             ("size", (15,)), ("far_z", (-100,)), ("gobo_tint", (1, 1, 1)),
                             ("gobo_anim_params", (0, 0, 1, 0)), ("proj_n", (0, 0, 1)),
                             ("proj_p", (0, 0, -5)), ("proj_range", (15,)), ("proj_lod", (1,)),
                             ("local_light_spec_exponent", (exponent,)),
                             ("relight_legacy_exponent", (exponent,)), ("relight_highlight_scale", (1,))]:
            self.uniform(name, *values)
        matrix = (C.c_float*16)(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, .5, .5, 1, 1)
        gl.matrix(gl.loc(program, b"proj_mat"), 1, 0, matrix)

    def tex(self, unit, w, h, data, internal=0x8814, fmt=0x1908):
        gl = self.gl
        tex = C.c_uint()
        gl.gen_tex(1, C.byref(tex))
        self.textures.append(tex)
        gl.active(0x84C0+unit)
        gl.bind_tex(0x0DE1, tex)
        gl.param(0x0DE1, 0x2801, 0x2600)
        gl.param(0x0DE1, 0x2800, 0x2601)
        gl.param(0x0DE1, 0x2802, 0x812F)
        gl.param(0x0DE1, 0x2803, 0x812F)
        gl.teximage(0x0DE1, 0, internal, w, h, 0, fmt, 0x1406,
                    None if data is None else data.ctypes.data)

    def uniform(self, name, *values):
        self.gl.uf[len(values)](self.gl.loc(self.program, name.encode()), *values)

    def integer(self, name, value):
        self.gl.ui(self.gl.loc(self.program, name.encode()), value)

    def prepare(self, radius, distance, pbr, component, rig=False):
        gl = self.gl
        gl.use(self.program)
        gl.bind_fbo(0x8D40, self.fbo)
        gl.viewport(0, 0, self.width, self.height)
        for unit, texture in zip((0, 2, 3, 1), self.textures):
            gl.active(0x84C0+unit)
            gl.bind_tex(0x0DE1, texture)
        self.uniform("testLight", 0, 0, distance)
        self.uniform("center", 0, 0, -4+distance)
        self.uniform("testShape", self.shape, 0, 1, -1)
        self.integer("relight_enabled", 1 if self.shape else 0)
        effective = 0 if rig else radius
        for name in ("testRadius", "local_light_size", "local_light_sizes[0]"):
            self.uniform(name, effective)
        self.integer("testPbr", pbr)
        self.integer("testComponent", component)
        gl.fn("glClearColor", None, *([C.c_float]*4))(0, 0, 0, 0)
        gl.fn("glClear", None, C.c_uint)(0x4000)

    def draw(self, *args, **kwargs):
        self.prepare(*args, **kwargs)
        self.gl.draw(4, 0, 3)

    def image(self, *args, **kwargs):
        self.draw(*args, **kwargs)
        pixels = np.empty((self.height, self.width, 4), np.float32)
        self.gl.read(0, 0, self.width, self.height, 0x1908, 0x1406, pixels.ctypes.data)
        assert np.all(np.isfinite(pixels)), "nonfinite production caller output"
        return pixels


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--assemble-only", action="store_true")
    ap.add_argument("--gpu", action="store_true")
    ap.add_argument("--cost", action="store_true")
    ap.add_argument("--viewer-closed-confirmed", action="store_true",
                    help="Use an explicit user confirmation if tasklist access is denied; a listed viewer still blocks")
    ap.add_argument("--fixtures-only", action="store_true", help="Rerun readbacks after the full permutation gate has passed")
    ap.add_argument("--matrix-only", action="store_true", help="Run the production permutation link gate without readbacks")
    ap.add_argument("--compile-only", action="store_true", help="Compile every production stage permutation without program linking; requires --matrix-only")
    ap.add_argument("--matrix-path", action="append", help="Select production families for diagnosing a link failure")
    ap.add_argument("--link-jobs", type=int, choices=range(1, 33), default=8,
                    help="Bounded ARB parallel compilation window; 1 uses serial linking")
    ap.add_argument("--path", action="append", choices=PATHS, help="Select caller readbacks; default runs every path")
    args = ap.parse_args()
    if args.compile_only and not args.matrix_only:
        ap.error("--compile-only requires --matrix-only; it cannot replace the readback/cost gate")
    out = ROOT / ".tmp/local_light_size_gl"
    out.mkdir(parents=True, exist_ok=True)
    assembled = {}
    for ref in ("HEAD", "WORKTREE"):
        for path in PATHS:
            file, defines, fragment = fixture(ref, path)
            (out / f"{ref}_{path}.frag").write_text(defines + fragment, encoding="utf-8")
            assembled[ref+"/"+path] = hashlib.sha256(fragment.encode()).hexdigest()
    if args.assemble_only:
        print("PASS assembled 22 baseline/current production caller fixtures; GPU not opened")
        return 0
    if not args.gpu:
        ap.error("choose --assemble-only or --gpu")
    status = subprocess.run(["tasklist", "/FI", "IMAGENAME eq AlchemyTest.exe", "/FO", "CSV", "/NH"],
                            timeout=15, capture_output=True, text=True)
    listed = '"AlchemyTest.exe"' in status.stdout
    confirmed = args.viewer_closed_confirmed and not listed
    if listed or ((status.returncode or not re.search(r"INFO: No tasks are running which match", status.stdout, re.I))
                  and not confirmed):
        print("PENDING-GPU: viewer closed status not established: " + (status.stdout+status.stderr).strip())
        return 3
    if confirmed:
        print("GPU lifecycle: explicit user confirmation that viewer is CLOSED; tasklist: " +
              (status.stdout+status.stderr).strip(), flush=True)
    import relight_3d_gl as G
    gl = G.GL()
    print("GL", gl.renderer, flush=True)
    extensions = gl.fn("glGetString", C.c_char_p, C.c_uint)(0x1F03) or b""
    parallel = (args.link_jobs > 1 and not args.compile_only and not args.fixtures_only and
                b"GL_ARB_parallel_shader_compile" in extensions.split())
    original_compile = gl.compile
    if parallel:
        # Khronos ARB_parallel_shader_compile: completion is queried for BOTH
        # shaders and programs before checking their ordinary success status.
        gl.fn("glMaxShaderCompilerThreadsARB", None, C.c_uint)(args.link_jobs)
        def compile_complete(shader):
            original_compile(shader)
            ready = C.c_int()
            while not ready.value:
                gl.getshader(shader, 0x91B1, C.byref(ready))
                if not ready.value:
                    time.sleep(.002)
        gl.compile = compile_complete
        print("Parallel native link window", args.link_jobs, flush=True)
    for cap in (0x0B71, 0x0BE2, 0x0C11, 0x8DB9, 0x0B44, 0x0BD0):
        gl.disable(cap)
    report = dict(renderer=gl.renderer, source_hashes=assembled, scenes=[],
                  production_gate="compile" if args.compile_only else "link",
                  fixtures_only=args.fixtures_only, parallel_link_jobs=args.link_jobs if parallel else 1)
    linkers = {ref: shader_linker(gl, ref, compile_only=args.compile_only) for ref in ("HEAD", "WORKTREE")}
    report["production_source_hashes"] = {ref: linker["source_hashes"] for ref, linker in linkers.items()}
    # Also link the complete production programs without fixture substitutions.
    for path in (() if args.fixtures_only else ("class3/deferred/pointLightF.glsl", "class3/deferred/multiPointLightF.glsl",
                 "class3/deferred/spotLightF.glsl", "class2/deferred/alphaF.glsl",
                 "class3/deferred/materialF.glsl", "class2/deferred/pbralphaF.glsl",
                 "class2/deferred/sharedActorFxPbrF.glsl")):
        linker = linkers["WORKTREE"]
        if args.matrix_path and Path(path).name not in args.matrix_path:
            continue
        variants = production_permutations(path)
        report.setdefault("production_permutations", {})[path] = variants
        pending = []
        completed = 0
        def progress():
            if completed % 25 == 0:
                print("Linkage progress", path, completed, "/", len(variants), flush=True)
        def reap_one():
            nonlocal completed
            while True:
                for index, (program, label) in enumerate(pending):
                    ready = C.c_int()
                    gl.getprog(program, 0x91B1, C.byref(ready))
                    if not ready.value:
                        continue
                    ok = C.c_int()
                    gl.getprog(program, 0x8B82, C.byref(ok))
                    if not ok.value:
                        message = C.create_string_buffer(64000)
                        gl.plog(program, len(message), None, message)
                        raise AssertionError(label+" LINK "+message.value.decode())
                    # Preserve the original linker's runtime-master bind check.
                    master = gl.loc(program, b"relight_enabled")
                    if master >= 0:
                        linker["use"](program)
                        for value in (1, 0, 1, 0):
                            gl.ui(master, value)
                    gl.fn("glDeleteProgram", None, C.c_uint)(program)
                    pending.pop(index)
                    completed += 1
                    progress()
                    return
                time.sleep(.002)
        for index, defines in enumerate(variants):
            label = path+"\n"+defines
            program = linker["link_production"](path, defines, label, defer_link=parallel)
            if parallel:
                pending.append((program, label))
                if len(pending) >= args.link_jobs:
                    reap_one()
            else:
                gl.fn("glDeleteProgram", None, C.c_uint)(program)
                completed += 1
                progress()
        while pending:
            reap_one()
        print("PASS production " + ("compilation" if args.compile_only else "linkage"), path, len(variants), "variants", flush=True)
        (out / "matrix_results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    if parallel:
        gl.fn("glMaxShaderCompilerThreadsARB", None, C.c_uint)(0)
        gl.compile = original_compile
    if args.matrix_only:
        return 0
    for path in (args.path or PATHS):
        runners = {}
        for ref in ("HEAD", "WORKTREE"):
            file, defines, fragment = fixture(ref, path)
            program = linkers[ref]["link_production"](file, defines, ref+"/"+path, fragment)
            runners[ref] = Runner(gl, program)
        pbrs = (0, 1) if path in PATHS[:6] else (0,) if path in ("alpha", "material") else (1,)
        for physical_source in (0.0, .05):
            for runner in runners.values():
                runner.shape = physical_source
            for pbr in pbrs:
                for distance in (.1, .3, 1):
                    for component in (0, 1):
                        baseline = runners["HEAD"].image(0, distance, pbr, component)
                        current = runners["WORKTREE"].image(0, distance, pbr, component)
                        if baseline.tobytes() != current.tobytes():
                            np.save(out / "identity_baseline.npy", baseline)
                            np.save(out / "identity_current.npy", current)
                            delta = np.abs(baseline-current)
                            index = np.unravel_index(delta.argmax(), delta.shape)
                            raise AssertionError((path, "zero differs from HEAD", pbr, distance, component,
                                                  physical_source, "max error", float(delta.max()),
                                                  "channels", int(np.count_nonzero(delta)), "pixel", index,
                                                  float(baseline[index]), float(current[index])))
                        peaks, widths = [], []
                        for size in (0, .05, .15, .3):
                            image = runners["WORKTREE"].image(size, distance, pbr, component)
                            if component == 0 or path == "alpha":
                                assert image.tobytes() == current.tobytes(), (path, "diffuse changed", size,pbr,distance,physical_source)
                            rig = runners["WORKTREE"].image(size, distance, pbr, component, rig=True)
                            assert rig.tobytes() == current.tobytes(), (path, "rig changed", size)
                            y = image[:, :, :3] @ np.array([.2126, .7152, .0722])
                            peak = float(y.max())
                            peaks.append(peak)
                            widths.append(int(np.count_nonzero(y >= peak*.5)))
                        if component == 1 and path != "alpha":
                            assert peaks[0] > 1.e-6, (path, "specular positive control missing",pbr,distance,physical_source,peaks)
                            assert peaks[-1] < peaks[0]*.99, (path, "size has no effect",pbr,distance,physical_source,peaks)
                            assert all(b <= a*(1+2.e-5)+2.e-7 for a, b in zip(peaks, peaks[1:])), (path, "peak rose", peaks)
                            assert widths[-1] >= widths[0], (path, "highlight did not broaden", widths)
                        report["scenes"].append(dict(path=path, pbr=pbr, distance=distance, component=component, physical_source=physical_source,
                                                     peaks=peaks, half_peak_pixels=widths))
        print("PASS production zero/HEAD, diffuse, rig, peak/width", path, flush=True)
    if args.cost:
        # Thirty-two actual production point lights, in the viewer's two
        # LIGHT_COUNT=16 batches (LL_DEFERRED_MULTI_LIGHT_COUNT is 16).
        # Paired ordering reverses every pair; measure p95 of paired GPU deltas.
        file, defines, fragment = fixture("WORKTREE", "multiPoint")
        # Undo only the analytic single-emitter INPUT adapters for this cost
        # scene. Distinct uniform array elements prevent common-expression
        # elimination from collapsing 32 lights into one repeated light.
        start = fragment.index("for (int light_idx")
        legacy = fragment.index("for (int i =", start)
        def arrays(text, index):
            text = text.replace("vec4(vec3(0,0,-4)+testLight,15)", f"light[{index}]")
            return text.replace("vec4(vec3(testLightPower),testFalloff)", f"light_col[{index}]")
        fragment = fragment[:start]+arrays(fragment[start:legacy], "light_idx")+arrays(fragment[legacy:], "i")
        assert "light[light_idx].xyz" in fragment and "light[i].xyz" in fragment
        defines = defines.replace("LIGHT_COUNT 1", "LIGHT_COUNT 16")
        states = {}
        cost_fbo = None
        for radius in (0.0, .15):
            batches = []
            for batch in range(2):
                program = linkers["WORKTREE"]["link_production"](file, defines, "4K32 cost batch", fragment)
                runner = Runner(gl, program, 3840, 2160)
                if cost_fbo is None:
                    cost_fbo = runner.fbo
                runner.fbo = cost_fbo
                for i in range(16):
                    index = batch*16+i
                    runner.uniform(f"light[{i}]", (index % 8-3.5)*.025, (index // 8-1.5)*.025, -3.5+index*.001, 15)
                    runner.uniform(f"light_col[{i}]", .5+index*.007, .7-index*.005, .6+index*.003, .5)
                    runner.uniform(f"local_light_sizes[{i}]", radius)
                runner.prepare(radius, .5, 1, 1)
                batches.append(runner)
            states[radius] = batches
        gl.fn("glEnable", None, C.c_uint)(0x0BE2)
        gl.fn("glBlendFunc", None, C.c_uint, C.c_uint)(1, 1)
        queries = (C.c_uint*2)()
        gl.gen_queries(2, queries)
        def paired(order):
            # Each executable is the same runtime-uniform shader, with its own
            # pre-uploaded light state. Queue BOTH GPU samples before waiting:
            # no CPU readback/upload/idle gap can separate this pair's draws.
            for index, size in enumerate(order):
                gl.use(states[size][0].program)
                gl.begin_query(0x88BF, queries[index])
                gl.draw(4, 0, 3)
                gl.use(states[size][1].program)
                gl.draw(4, 0, 3)
                gl.end_query(0x88BF)
            values = {}
            for index, size in enumerate(order):
                ns = C.c_uint64()
                gl.query_u64(queries[index], 0x8866, C.byref(ns))
                values[size] = ns.value*1.e-6
            return [values[0], values[.15]]
        for i in range(12):
            paired((0, .15) if i % 2 == 0 else (.15, 0))
        pairs = [paired((0, .15) if i % 2 == 0 else (.15, 0)) for i in range(80)]
        p95 = float(np.percentile([b-a for a, b in pairs], 95))
        p95_zero, p95_sized = [float(x) for x in np.percentile(pairs, 95, axis=0)]
        report["cost"] = dict(resolution=[3840, 2160], lights=32, batches=2, lights_per_batch=16, paired_ms=pairs,
                              p95_zero_ms=p95_zero, p95_sized_ms=p95_sized,
                              delta_of_p95_ms=p95_sized-p95_zero, p95_delta_ms=p95, limit_ms=.1)
        (out / "results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
        assert max(p95, p95_sized-p95_zero) <= .1, ("4K32 p95 size cost exceeded .1 ms", p95, p95_sized-p95_zero)
        print("PASS 4K32 paired GPU p95 delta", p95, "ms")
    (out / "results.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("PASS production local light size GPU fixture")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
