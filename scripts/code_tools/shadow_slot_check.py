"""Shadow-slot wiring and real-WGL filter regression; no viewer build or launch."""
from pathlib import Path
import sys
import re

ROOT = Path(__file__).resolve().parents[2]
NV = ROOT / "indra/newview"
sys.path.insert(0, str(Path(__file__).resolve().parent))


def check_wiring(pipe, agent, shader):
    assert shader.count("uniform vec4 area_fade") == 1
    final_fade = shader[shader.index("// After BOTH the cube combine"):]
    assert "if (all(equal(area_axis,vec2(0))) && area_world_debug == 0)" in final_fade
    assert "uniform vec4 area_fade = vec4(1.0)" in shader
    assert "if (area_fade[c] < 1.0)" in shader and shader.index("if (area_fade[c] < 1.0)") > shader.rindex("else frag_color[c] = min(")
    assert 'uniform4f(area_fade,1.f,1.f,1.f,1.f)' in pipe
    assert 'uniform4fv(area_fade,1,' in pipe
    assert "upsample && steady_main ? final_fade.mV : identity_fade.mV" in pipe
    bindings = list(re.finditer(r"\bgAreaShadowFilterProgram\.bind\(\)|\bbindDeferredShader\(gAreaShadowFilterProgram\)", pipe))
    assert len(bindings) == 2, "audit any new filter caller"
    for binding in bindings:
        draw = pipe.index("mScreenTriangleVB->", binding.end())
        assert 'static const LLStaticHashedString area_fade("area_fade");' in pipe[binding.end():draw], "unset/static fade at filter bind"
    native = pipe[pipe.index("void LLPipeline::retainSpotShadowOwners()"):pipe.index("// [BDMerge G3.3] Side-effect-free variant")]
    assert "LLDrawable* owner = mShadowSpotLight[i];" in native
    assert "LLDrawable* target = mSpotAuctionMain[i].drawable;" in native
    assert "ranked[i] = mSpotAuctionMain[i].light;" in native
    assert "owners[i] = mSpotAuctionOwnerScores[i];" in native
    assert "targets[i] = mSpotAuctionMain[j].drawable;" in native
    snapshot = pipe[pipe.index("void LLPipeline::snapshotSpotShadowAuction()"):pipe.index("void LLPipeline::retainSpotShadowOwners()")]
    assert "if (!isLiveMainAreaShadowPass()) return;" in snapshot
    assert "mSpotAuctionMain[i] = {target, {volume->getID(), volume->getSpotLightPriority(), spotShadowTier(volume)}};" in snapshot
    assert "if (isLiveMainAreaShadowPass() && s_idx >= 0" in pipe
    assert "mSpotAuctionOwnerScores[s_idx] = {volume->getID(), m_pri, spotShadowTier(volume)};" in pipe
    assert pipe.count("mSpotAuctionEligible[i] = NULL;") == 0, "secondary stock reset must not clear main eligibility"
    lighting = pipe[pipe.index("void LLPipeline::renderDeferredLighting("):pipe.index("void LLPipeline::renderPrismDisplays(")]
    assert 'snapshotSpotShadowAuction();' in lighting
    assert lighting.rindex("setupSpotLight(") < lighting.index("snapshotSpotShadowAuction();") < lighting.index("fullscreen_lights.clear();")
    resize = pipe[pipe.index("void LLPipeline::resizeScreenTexture()"):pipe.index("bool LLPipeline::allocateScreenBuffer(")]
    assert "if (mRT == &mMainRT) resetShadowSlotHistory();" in resize
    assert "isLiveMainAreaShadowPass()" not in resize
    assert "selected.addIncumbent(candidate," in pipe and "channel.score = ALAreaShadow::incumbentScore" not in pipe
    assert "extent.splat(range);" in pipe and "ALAreaShadow::candidateRadius(volume->getLightRadius(),true,true)" in pipe
    assert "const bool steady_main = pick_record_pass && slot_hysteresis();" in pipe
    assert agent.count("gPipeline.resetShadowSlotHistory();") == 1
    assert agent.count("gPipeline.resetShadowSlotTimers();") == 1
    teleport = agent.index("void LLAgent::handleTeleportFinished()")
    crossing = agent.index("if (mRegionp != regionp)")
    assert "resetShadowSlotHistory" in agent[teleport:teleport+150]
    assert "resetShadowSlotTimers" in agent[crossing:crossing+150]
    reset = pipe[pipe.index("void LLPipeline::resetShadowSlotHistory()"):pipe.index("void LLPipeline::resetShadowSlotTimers()")]
    timers = pipe[pipe.index("void LLPipeline::resetShadowSlotTimers()"):pipe.index("// Both existing residency")]
    for field in ("mAreaShadowPrevious = {}", "mAreaShadowRamps = {}", "mShadowSlotSeed = true", "resetShadowSlotTimers();"):
        assert field in reset
    assert "mSpotChallenge = {}" in timers and "mWorldNativeRelease" in timers
    assert "mAreaShadow" not in timers and "mShadowSpotLight" not in timers and "mSpotLightFade" not in timers
    assert "else if (owner && owner->isDead())" in native
    assert "mShadowSlotSeed" not in native, "reset frame must bypass reconciliation, not null targets"
    assert "const bool retain_slots = ALShadowSlot::retainNativeFrame(slot_hysteresis(),stable_spots(),mShadowSlotSeed);" in pipe
    assert "if (retain_slots) retainSpotShadowOwners();" in pipe
    assert "if (retain_slots)\n                fade_amt =" in pipe
    assert "mSpotAuctionFrame = gFrameCount; // Pure live-main view test above; independent of soft shadows being enabled." in pipe
    assert "const auto previous_channels = mAreaShadowPrevious;" in pipe
    assert "const auto previous_ramps = mAreaShadowRamps;" in pipe
    assert "return previous_channels.contains(id,gFrameCount,selection_time) || previous_ramps.contains(id,gFrameCount,selection_time);" in pipe
    assert "ALAreaShadow::incumbentAdmittedStrength(selected.lights[i].score,dropped_score,true,true)" in pipe
    gather = pipe[pipe.index("const U32 wanted_lights = selected.size;"):pipe.index("// Explicit on every bind: secondary/off/blur")]
    assert "admission[i] = slot_exiting[i] ? 1.0 :" in gather
    assert "mAreaShadowPrevious.strength[i] = admission[i] * slot_fade[i];" in gather
    assert "dropped_score = ALAreaShadow::droppedScore(desired_selection,normal_limit,ramps);" in gather
    assert "selected = ALAreaShadow::rampSelection(desired_selection,desired_count,ramps,candidate_for);" in gather
    assert "ALAreaShadow::regularShare(selected.lights[i].score,regular_score)" in gather
    assert pipe.index("const double regular_score =") < pipe.index("const auto ramps = mAreaShadowRamps.update")
    assert "mAreaShadowRamps.contains(candidate.id, gFrameCount, selection_time)" in pipe
    assert "true,selection_time,gFrameCount,[&](const LLUUID& id)" in pipe
    assert "mAreaShadowPrevious.time = selection_time;" in pipe
    assert "previous_channels.effectiveStrength(id,gFrameCount,selection_time)" in pipe
    assert 'if (!recent_auction) { mSpotChallenge = {}; return; }' in native
    early = native.index("if (!recent_auction) { mSpotChallenge = {}; return; }")
    assert early < native.index("for (U32 i = 0; i < count; ++i)")
    assert "dropout.mV[i] = static_cast<F32>(1.0-admission[channel]);" in gather
    assert "dropout.mV[i] = 0.f;" in gather
    assert not re.search(r"dropout[^;]*slot_fade",gather)
    setup = pipe[pipe.index("bool LLPipeline::allocateScreenBufferInternal("):pipe.index("//allocate deferred rendering color buffers")]
    assert 'RenderShadowSlotHysteresis' not in setup, "no feature VRAM allocation"
    assert "sparePlaneCost" not in pipe and "spare_ready" not in pipe
    assert "const bool spare_channel = ALShadowSlot::spareChannel(desired_count,kept);" in pipe
    assert "static_cast<U32>(llclamp(cap(),1,8)) > 4 ? 2u : 1u" in pipe
    assert pipe.index("const U32 kept =") < pipe.index("const auto ramps = mAreaShadowRamps.update")
    assert "},kept);" in pipe
    for target in ("areaGather", "areaScratch", "areaVisibility"):
        assert "mRT->%s[1].isComplete()" % target in pipe
        assert "mRT->%s[1].getWidth() ==" % target in pipe and "mRT->%s[1].getHeight() ==" % target in pipe
    assert "const bool extra_allowed = spare_channel && retained_ready &&" in pipe
    assert "mAreaShadowRamps.update(desired,desired_count,desired_count,extra_allowed," in pipe
    assert "if (ramps.size > desired_count) work = extra_work;" in pipe
    assert "mWorldShadowReason[k] == ALWorldShadow::SlotReason::Rendered" in pipe
    callback = pipe[pipe.index("ALWorldShadow::updateResidencyPicked(mWorldShadowResident"):pipe.index("take.worldOn = cube_main;")]
    cubes = pipe[pipe.index("void LLPipeline::generateWorldShadowCubes("):]
    assert "return worldShadowResidentEligible(id, native_sampled);" in callback
    assert "if (!worldShadowResidentEligible(mWorldShadowResident.id[k], native_mapped))" in cubes
    assert "const bool resident = live && cube_main && mWorldShadowResident.contains(id);" in pipe
    assert "RampStatus{live,resident && !ready,ready,rendering," in pipe
    assert "ALAreaShadow::rampPlanMatches(work,extra_plan,desired_count+1)" in pipe
    assert "mAreaShadowRamps.retainRendered(rendered,channels);" in pipe
    assert "ALAreaShadow::candidateRadius(radius,true,true)" in cubes


pipe = (NV / "pipeline.cpp").read_text()
agent = (NV / "llagent.cpp").read_text()
shader_path = NV / "app_settings/shaders/class3/deferred/areaShadowFilterF.glsl"
shader_text = shader_path.read_text()
check_wiring(pipe, agent, shader_text)
mutations = [
    (pipe.replace("mAreaShadowRamps.retainRendered(rendered,channels);", ""), agent, shader_text),
    (pipe.replace("ALAreaShadow::rampPlanMatches(work,extra_plan,desired_count+1)", "true"), agent, shader_text),
    (pipe.replace("ALAreaShadow::candidateRadius(radius,true,true)", "radius * 1.5"), agent, shader_text),
    (pipe.replace("LLDrawable* owner = mShadowSpotLight[i];", "LLDrawable* owner = mTargetShadowSpotLight[i];"), agent, shader_text),
    (pipe.replace("return worldShadowResidentEligible(id, native_sampled);", "return true;"), agent, shader_text),
    (pipe.replace("if (!worldShadowResidentEligible(mWorldShadowResident.id[k], native_mapped))", "if (native_mapped)"), agent, shader_text),
    (pipe.replace("mAreaShadowRamps = {};", ""), agent, shader_text),
    (pipe, agent.replace("gPipeline.resetShadowSlotTimers();", "gPipeline.resetShadowSlotHistory();", 1), shader_text),
    (pipe, agent[::-1].replace(";)(yrotsiHtolSwodahSteser.enilepiPg", "", 1)[::-1], shader_text),
    (pipe.replace("upsample && steady_main ? final_fade.mV : identity_fade.mV", "final_fade.mV"), agent, shader_text),
    (pipe.replace("ALAreaShadow::candidateRadius(volume->getLightRadius(),true,true)", "range"), agent, shader_text),
    (pipe, agent, shader_text.replace("if (area_fade[c] < 1.0)", "if (true)")),
]
mutations += [
    (pipe.replace("const bool resident = live && cube_main && mWorldShadowResident.contains(id);", "const bool resident = live && cube_main && !candidate.shadowMap && mWorldShadowResident.contains(id);"), agent, shader_text),
    (pipe.replace("else if (owner && owner->isDead())", "else"), agent, shader_text),
    (pipe.replace("mShadowSlotSeed = true;", "mShadowSlotSeed = false;"), agent, shader_text),
    (pipe.replace("ALShadowSlot::retainNativeFrame(slot_hysteresis(),stable_spots(),mShadowSlotSeed)", "slot_hysteresis() && !stable_spots()"), agent, shader_text),
    (pipe.replace("dropout.mV[i] = static_cast<F32>(1.0-admission[channel]);", "dropout.mV[i] = static_cast<F32>(1.0-admission[channel]*slot_fade[channel]);"), agent, shader_text),
    (pipe.replace("const bool extra_allowed = spare_channel && retained_ready &&", "const bool extra_allowed ="), agent, shader_text),
    (pipe.replace("if (ramps.size > desired_count) work = extra_work;", ""), agent, shader_text),
    (pipe.replace('    //allocate deferred rendering color buffers', '    if (gSavedSettings.getBOOL("RenderShadowSlotHysteresis")) mRT->areaGather[1].allocate(resX,resY,GL_RGBA8,false);\n    //allocate deferred rendering color buffers'), agent, shader_text),
    (pipe.replace("ALAreaShadow::incumbentAdmittedStrength(selected.lights[i].score,dropped_score,true,true)", "ALAreaShadow::admittedStrength(selected.lights[i].score,dropped_score)"), agent, shader_text),
    (pipe.replace("return previous_channels.contains(id,gFrameCount,selection_time) || previous_ramps.contains(id,gFrameCount,selection_time);", "return mAreaShadowPrevious.contains(id,gFrameCount,selection_time) || mAreaShadowRamps.contains(id,gFrameCount,selection_time);"), agent, shader_text),
    (pipe.replace("mWorldShadowReason[k] == ALWorldShadow::SlotReason::Rendered", "true"), agent, shader_text),
    (pipe.replace('static const LLStaticHashedString area_fade("area_fade");','const LLStaticHashedString area_fade("area_fade");'), agent, shader_text),
]
mutations += [
    (pipe.replace("if (!recent_auction) { mSpotChallenge = {}; return; }", "if (!recent_auction) { mSpotChallenge = {}; }"), agent, shader_text),
    (pipe.replace("mAreaShadowPrevious.strength[i] = admission[i] * slot_fade[i];", "mAreaShadowPrevious.strength[i] = slot_fade[i];"), agent, shader_text),
    (pipe.replace("dropped_score = ALAreaShadow::droppedScore(desired_selection,normal_limit,ramps);", ""), agent, shader_text),
    (pipe.replace("selected = ALAreaShadow::rampSelection(desired_selection,desired_count,ramps,candidate_for);", "selected = desired_selection;"), agent, shader_text),
    (pipe.replace("admission[i] = slot_exiting[i] ? 1.0 :", "admission[i] ="), agent, shader_text),
    (pipe.replace("ALAreaShadow::regularShare(selected.lights[i].score,regular_score)", "ALAreaShadow::regularShare(selected.lights[i].score,selected.lights[work.lead].score)"), agent, shader_text),
    (pipe.replace("mAreaShadowRamps.contains(candidate.id, gFrameCount, selection_time)", "mAreaShadowRamps.contains(candidate.id, gFrameCount, LLTimer::getTotalSeconds())"), agent, shader_text),
    (pipe.replace("previous_channels.effectiveStrength(id,gFrameCount,selection_time)", "1.0"), agent, shader_text),
]
mutations += [
    (pipe.replace("incumbentAdmittedStrength(selected.lights[i].score,dropped_score,true,true)", "incumbentAdmittedStrength(selected.lights[i].score,dropped_score,incumbent(selected.lights[i].id),true)"), agent, shader_text),
    (pipe.replace("const bool spare_channel = ALShadowSlot::spareChannel(desired_count,kept);", "const bool spare_channel = ALShadowSlot::spareChannel(desired_count);"), agent, shader_text),
    (pipe.replace("},kept);", "});"), agent, shader_text),
]
for target in ("areaGather", "areaScratch", "areaVisibility"):
    mutations.append((pipe.replace("mRT->%s[1].isComplete()" % target, "true"), agent, shader_text))
mutations += [
    (pipe.replace("if (isLiveMainAreaShadowPass() && s_idx >= 0", "if (s_idx >= 0"), agent, shader_text),
    (pipe.replace("if (!isLiveMainAreaShadowPass()) return;", ""), agent, shader_text),
    (pipe.replace("snapshotSpotShadowAuction(); // After every", "/* missing snapshot */ // After every"), agent, shader_text),
    (pipe.replace("LLDrawable* target = mSpotAuctionMain[i].drawable;", "LLDrawable* target = mTargetShadowSpotLight[i];"), agent, shader_text),
    (pipe.replace("owners[i] = mSpotAuctionOwnerScores[i];", "owners[i] = {volume->getID(),volume->getSpotLightPriority(),spotShadowTier(volume)};"), agent, shader_text),
    (pipe.replace("mTargetShadowSpotLight[i] = NULL;\n                }", "mTargetShadowSpotLight[i] = NULL; mSpotAuctionEligible[i] = NULL;\n                }"), agent, shader_text),
    (pipe.replace("if (mRT == &mMainRT) resetShadowSlotHistory();", "if (isLiveMainAreaShadowPass()) resetShadowSlotHistory();"), agent, shader_text),
    (pipe, agent, shader_text.replace("// After BOTH the cube combine and artistic look. Diagnostics remain raw; identity preserves today's bytes.\n    if (all(equal(area_axis,vec2(0))) && area_world_debug == 0)", "// After BOTH the cube combine\n    if (all(equal(area_axis,vec2(0))))")),
]
for index, args in enumerate(mutations):
    try:
        check_wiring(*args)
    except AssertionError:
        continue
    raise AssertionError("wiring mutant survived: %d" % index)
print("shadow slot wiring: PASS, %d mutants rejected" % len(mutations), flush=True)

# Reuse the project's production GLSL compiler/linker and hidden WGL fixture. Only the
# fixture setup prefix is needed; all shader modules and interfaces are real production.
check = Path(__file__).with_name("relight_shader_check.py")
scope = {"__file__": str(check), "__name__": "shadow_slot_gpu"}
prefix = check.read_text().split("if '--area-only' in sys.argv:")[0]
exec(compile(prefix, str(check), "exec"), scope)
for version in (330, 400, 420):
    # Same module/permutation loader, with each supported non-world/world GLSL branch.
    compile_source = prefix[prefix.index("def compile_part("):prefix.index("files=[")]
    exec(compile(compile_source.replace("#version 420", "#version %d" % version), "slot_version", "exec"), scope)
    scope["link_production"]("class3/deferred/areaShadowFilterF.glsl", "", "slot filter GLSL %d" % version)
    print("slot filter compile/link GLSL", version, "PASS", flush=True)
exec(compile(prefix[prefix.index("def compile_part("):prefix.index("files=[")], "slot_version", "exec"), scope)
fixture = check.with_name("relight_area_check.py")
exec(compile(fixture.read_text().split("# Thin-only")[0], str(fixture), "exec"), scope)

C, np = scope["C"], scope["np"]
loc, uf, ui, matrix = (scope[k] for k in ("loc", "uf", "ui", "matrix"))
base = scope["area_filter"]
get_uniform = scope["gl"]("glGetUniformfv", None, scope["U"], scope["I"], C.POINTER(scope["F"]))
default = (scope["F"] * 4)()
get_uniform(base, loc(base, b"area_fade"), default)
assert list(default) == [1.0] * 4, list(default)
size = scope["area_size"]
input_pixels = np.empty((size, size, 4), np.float32)
input_pixels[:] = [0.2, 0.3, 0.4, 0.5]
scope["area_texture"](3, input_pixels)


def render(pr, fades, world=False, debug=0):
    scope["area_use"](pr)
    matrix(loc(pr, b"inv_proj"), 1, 0, scope["inverse"].ctypes.data_as(C.POINTER(scope["F"])))
    ui(loc(pr, b"area_source"), 3)
    ui(loc(pr, b"depthMap"), 1)
    ui(loc(pr, b"normalMap"), 2)
    ui(loc(pr, b"area_debug_channel"), 0)
    ui(loc(pr, b"area_world_debug"), debug)
    uf[2](loc(pr, b"area_axis"), 0.0, 0.0)
    uf[1](loc(pr, b"area_radius"), 0.0)
    uf[3](loc(pr, b"area_look"), 0.0, 0.0, 0.0)
    uf[4](loc(pr, b"area_world"), float(world), 0.0, 0.0, 0.0)
    uf[4](loc(pr, b"area_fade"), *fades)
    return scope["area_read"]()


block = """    // After BOTH the cube combine and artistic look. Diagnostics remain raw; identity preserves today's bytes.
    if (all(equal(area_axis,vec2(0))) && area_world_debug == 0)
        for (int c = 0; c < 4; ++c)
            if (area_fade[c] < 1.0) frag_color[c] = mix(1.0,frag_color[c],area_fade[c]);
"""
assert shader_text.count(block) == 1
original_read = scope["read_shader"]


def link_text(text, label):
    scope["read_shader"] = lambda path: text if Path(path).name == shader_path.name else original_read(path)
    try:
        return scope["link_production"]("class3/deferred/areaShadowFilterF.glsl", "", label)
    finally:
        scope["read_shader"] = original_read


no_fade = link_text(shader_text.replace(block, ""), "identity negative control")
reference = render(no_fade, (1, 1, 1, 1))
assert render(base, (1, 1, 1, 1)).tobytes() == reference.tobytes()
assert np.all(render(base, (0, 0, 0, 0)) == 255)
assert render(base, (1, 1, 1, 1)).tobytes() == reference.tobytes(), "stale ramp after identity reset"
gradient = (np.arange(size*size*4,dtype=np.uint32).reshape(size,size,4)%256).astype(np.float32)/255.0
scope["area_texture"](3, gradient)
assert render(base, (1, 1, 1, 1)).tobytes() == render(no_fade, (1, 1, 1, 1)).tobytes(), "identity over every 8-bit visibility"
scope["area_texture"](3, input_pixels)
reference = render(no_fade, (1, 1, 1, 1))

# Keep the actual final combine and fade. Stub only the expensive cube lookup to a
# known dark visibility, independent of atlas geometry, to discriminate blend ORDER.
assert shader_text.count("float awVisibility(int c)") == 1
cube_text = shader_text.replace("float awVisibility(int c)", "float awVisibilityActual(int c)")
cube_text = cube_text.replace("void main()", "float awVisibility(int c) { return 0.2; }\nvoid main()", 1)
cube_program = link_text(cube_text, "post-cube fade fixture")
for fade, expected in ((0.0, 255), (0.5, 153), (1.0, 51)):
    pixels = render(cube_program, (fade, 1, 1, 1), True)
    assert np.max(abs(pixels[:, :, 0].astype(int) - expected)) <= 1, (fade, pixels[0, 0])
    assert np.array_equal(pixels[:, :, 1:], reference[:, :, 1:]), "other channels changed"
for name, text in (("removed", cube_text.replace(block, "")),
                   ("before_cube", cube_text.replace(block, "").replace("    // World-owned channels:", block + "    // World-owned channels:", 1))):
    mutant = link_text(text, "fade mutant " + name)
    assert not np.all(render(mutant, (0, 1, 1, 1), True)[:, :, 0] == 255), name
print("post-resolve fade: PASS, identity bytes/stale reset/independent channels/cube endpoints; 2 GPU mutants rejected", flush=True)

# Composed screen/contact exit: gather keeps admission=0.5 (visibility=0.6),
# then final resolve's fade=0.5 gives 0.8. A second gather fade would give 0.9.
input_pixels[:] = [0.6,0.3,0.4,0.5]
scope["area_texture"](3,input_pixels)
linear = render(base,(0.5,1,1,1))
assert np.max(abs(linear[:,:,0].astype(int)-204)) <= 1, linear[0,0]
print("composed screen/contact exit: PASS, linear post-resolve fade",flush=True)

# Diagnostics retain raw values at every fade; stub only lookup values, as above.
debug_text = cube_text.replace("float awDebugValue(int c, int mode)", "float awDebugValueActual(int c, int mode)")
debug_text = debug_text.replace("void main()", "float awDebugValue(int c, int mode) { return 0.35; }\nvoid main()", 1)
debug_program = link_text(debug_text,"unfaded diagnostic fixture")
for mode in (1,2,3):
    expected = render(debug_program,(1,1,1,1),True,mode)
    for fade in (0.0,0.5):
        assert render(debug_program,(fade,fade,fade,fade),True,mode).tobytes()==expected.tobytes(),("dimmed diagnostic",mode,fade)
unguarded = link_text(debug_text.replace(block,block.replace(" && area_world_debug == 0","")),"diagnostic fade mutant")
assert render(unguarded,(0,1,1,1),True,1).tobytes()!=render(debug_program,(0,1,1,1),True,1).tobytes()
print("raw world diagnostics: PASS modes 1/2/3, zero/half/identity fade; 1 GPU mutant rejected",flush=True)
