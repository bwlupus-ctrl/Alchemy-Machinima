# Avatar Light Response — design (per-avatar, light-independent material trim)

Status: DESIGN ONLY, revision R3 — cleared for implementation (2026-09-29). Implement in the order of §15. Branch `fix/animesh-clone-pose-polish`, HEAD `1ecdbbdc35c`; all
`file:line` anchors are at that HEAD. Implementer: Sonnet. Review: adversarial loop to 0 must-fix, then ONE build
(CLAUDE.md). Ships whole: engine + shaders + UI + menus + persistence + instrumentation in one delivery.

## R1 — resolution of Codex review round 1 (GO-after-fixes, no P0)

| # | Finding | Resolution (where) |
|---|---|---|
| P1-1 | Scaling F0 raises punctual diffuse via `(1−F)` (deferredUtil.glsl:781; 2.544× at VdotH .1, Tame 100 %); IBL got s²…s | Tame never touches F0/specularColor/radiance. The EVALUATED specular (`specPunc`, `iblSpec`) is × s after the BRDF for sun, every local/rig light and IBL; diffuse and Fresnel untouched. Forwarding variants `pbrBaseLightKeep` / `pbrCalcPointLightOrSpotLightRimKeep` (old names forward 1.0). §1.2, §7.4, §14 |
| P1-2 | Alpha-impostor programs (llviewershadermgr.cpp:2855) have no Actor FX link and `FOR_IMPOSTOR` skips the trim; deferred proxy clones keep overlay sweeps through actorghostF (llactormover.cpp:12759) | Adjusted avatars are kept live (R2 P1-A mechanism); bakes upload identity. actorghostF CLONE sweeps get Brightness (beauty sweeps) and Glow (glow sweep). §2.3, §5.6, §7.5 |
| P1-3 | sharedActorFxPbrGlowF.glsl:297 authored bloom excluded; graphic Cover classifies untrimmed authored source (actorFxF.glsl:1526-1528) | `authored_lum` trimmed; graphic Cover handled by the R2 P1-C formula. §7.3 |
| P1-4 | Shared replay uploads `proxy.mStyleId` though commands include worn animesh | Per-command response from the command's own draw-info owners (R2: via a style-only entry point). §5.5 |
| P1-5 | Removing bypassed/identity entries let lookup fall through | Explicit entries (incl. identity/bypass) terminate the chain; absent = inherit; UI Reset vs Inherit. §4 |
| P2 | Gloss quantization; spec-AO; EV/RTGI precision; proof wording | §1.2, §9.1, §10-§12 |

## R2 — resolution of Codex review round 2 (Inherit, authored bloom, Keep helpers verified)

| # | Finding | Resolution (where) |
|---|---|---|
| P1-A | "Never impostor" ignored animation throttling: `computeUpdatePeriod` (llvoavatar.cpp:4630-4690) sets 32/48/64-frame periods independently of `isImpostor()` | The single avatar-policy edit is in `computeUpdatePeriod`, after the Actor FX early-out (:4633-4637): `if (!isVisuallyMuted() && ALAvatarLightResponse::keepsLive(this)) { mUpdatePeriod = 1; return; }`. `isImpostor()` (:12399-12420) is `visually_muted \|\| (sLimitNonImpostors && mUpdatePeriod > 1)`, so it follows with NO edit; mute list, Never Render, too-complex / too-slow jellydoll stay authoritative (we deliberately do not bypass `isVisuallyMuted`, unlike Actor FX at :4199-4205). Crowd budget: adjusted subjects bypass `sMaxNonImpostors` (:616, `RenderAvatarMaxNonImpostors`) — decision: no cap, amber floater warning when adjusted-and-visible > that limit. §2.3, §8.1 |
| P1-B | CLONE Glow hit `ghostGlowOnly` (synthetic `worldBloomRadiance`), but CLONE authored emission is the additive RGB `SWEEP_GLOW` (llactormover.cpp:12815-12822) | Glow applies to `SWEEP_GLOW` only: the CPU uploads a GLOW slot (`alrParams.z := g`) for that sweep, so one shader multiply serves both sweeps and Brightness never reaches emission. `ghostGlowOnly` untouched. Insertion moved OUT of `GHOST_WORLD_PASS`: after the `if (ghostWorldLinear != 0) {…}` block (actorghostF.glsl:1457-1471), before `#ifdef GHOST_WORLD_PASS` :1472, with a linear-light branch for display-space overlays. Only Ghost Studio draws (non-null `mLightResponseKey`) are trimmed; Actor FX legacy/system-body Cover replays through actorghost programs stay identity by design. §5.6, §7.5 |
| P1-C | Partial graphic Cover double-trim: `actorFxPbrPostLight` returns `mix(layer_source, fx, strength)` (actorFxF.glsl:1948) with `layer_source = lit_color` (:1489, never mutated) already trimmed | Trim only the fx term, algebraically and division-free: `c' = c + (k − 1)·(c − (1 − σ)·lit_pre)` = `(1−σ)·lit_pre + σ·k·fx`, σ = graphic-cover strength (0 otherwise). New helper `actorFxGraphicCoverStrength()` (fallback 0). §7.3 |
| P2 | Proxy→pet uniform thrash, inflated counts | Public `uploadActorFxStyleOnly(...)`; shared-replay sites call it plus exactly ONE per-command response upload. Uploads counted only when the bound program has an active `alrParams` location. §5 |
| P2 | Albedo tint can be hidden | Debug mode 3 is an OVERRIDE: albedo := magenta, roughness 1, specular off, emission := 4× magenta (deferred; R3 made it a replace); final colour := magenta (forward, graphic Cover, actorghost). T1 conditional on the emissive buffer. §6, §7, §12 |
| P2 | Gloss rule §6 ≠ §1.2 | One rule, skipped at d == 0. §1.2, §6 |

## R3 — resolution of Codex review round 3 (P1-A, P1-B, P1-C algebra and R2 P2s pass)

| # | Finding | Resolution (where) |
|---|---|---|
| P1 | Ghost uploads named the wrong program: `drawGeometryGhost` binds `batch_shader` (indexed vs plain, llactormover.cpp:12025-12037) for batches and `static_shader` (:12499) for static faces; `uniform4fv` uses the named object's locations/cache while `glUniform*` hits the bound program | Upload to `*batch_shader` inside `draw_batches` and to `*static_shader` inside `draw_static`, AFTER `apply_program(...)` and BEFORE the draw call, for every draw including identity resets. §5.6 |
| P2 | Debug override not final: actorghost override sat before signal modulation / additive radiance / fog (actorghostF.glsl:1477-1537); deferred used `emissive += 4·magenta` | actorghost: override immediately before the final beauty write (:1539). Deferred writers REPLACE emission (`frag_data[3].rgb = 4·magenta`) in debug. T1 qualified for alpha compositing and post-processing. §6, §7, §7.5, §12 |

User decisions (2026-09-29): per-account save ON by default; Glow trims creator-authored glow only (not Actor FX look
emission); clones inherit their source unless given their own entry.

## 1. What the user asked for

"Some avatars reflect way more light than others … not per light … not every avatar will be lit by my lights."
A trim that changes how ONE avatar responds to ALL light (sun/moon, sky ambient, probe irradiance + radiance, SSR,
hero mirrors, local lights, Cine Light Rig lights), for ANY avatar in view (residents, self, animesh, Ghost Studio
clones), independent of the Director cast and of any light.

### 1.1 Controls (per target). Defaults are exact identity.

| UI control | Symbol | Range | Default | Meaning |
|---|---|---|---|---|
| Diffuse | d | 0–100 % | 0 | PBR `r' = mix(r, 1, d)` (perceptual roughness); legacy gloss reduction |
| Tame reflections | t | 0–100 % | 0 | evaluated specular (punctual + IBL, incl. SSR/hero) × `s = 1 − t`; legacy spec colour + env × s |
| Brightness | e | −2 … +1 EV, 0.05 step | 0 | diffuse albedo × `k = 2^e` (hue-preserving ceiling when brightening) |
| Glow | g | 0–200 % | 100 | creator-authored emissive colour and glow × g |
| Bypass (A/B) | — | checkbox | off | explicit identity (still blocks fallback) |

Master: `AvatarLightResponseEnabled` (default TRUE; an empty table is already inert).

### 1.2 Math (single source of truth; mirrored in the pure model for tests)

- Packed per draw: `alrParams = vec4(d, s = 1 − t, k = exp2(e), g)`, CPU-clamped (d,t ∈ [0,1], e ∈ [−2,1], g ∈ [0,2]; non-finite → default). Glow slot (actorghost `SWEEP_GLOW` only): `vec4(d, s, g, g)`.
- **Diffuse (PBR):** linear in perceptual roughness (`α = r'²`; r .2, d .5 → r' .6, α .36). GGX spreads the lobe and
  lowers its peak — no diffuse gain. Side effect: `computeSpecularAO(nv, ao, α)` in pbrIbl (deferredUtil.glsl:663)
  depends on roughness, so authored AO darkens IBL specular slightly differently. Documented, not compensated.
- **Diffuse (legacy):** `1 − gloss` is legacy perceptual roughness. Exact rule (identical in §6 and the model):
  `if (d > 0 && gloss >= 0.5/255) gloss' = max(gloss·(1 − 0.95·d), 1/255); else gloss' = gloss`. A gloss that
  survives the RGBA8 specular attachment (≥ half a code) still survives, so the `spec.a > 0.0` branches
  (softenLightF.glsl:257, local-light legacy branches) never flip. Blinn-Phong LUT is normalized
  (pipeline.cpp:2521-2541): broader, dimmer highlight.
- **Tame (PBR):** F0, `specularColor`, `radiance` are NOT modified. After BRDF evaluation: `specPunc *= s` (sun inside
  pbrBaseLight, each local light after its `pbrPunctual`), `iblSpec *= s` (inside pbrBaseLight after `pbrIbl`).
  Punctual diffuse `(1−F)·diffuse` (deferredUtil.glsl:781) and IBL diffuse are bit-unchanged ⇒ exactly "× s on
  specular" for dielectrics AND metals. `radiance` holds probes, SSR and hero mirror (class3 reflectionProbeF.glsl
  `doProbeSample` :763-806: probes :777, SSR :798, hero :804) ⇒ all reflected environment × s. No LUT fetch.
- **Tame (legacy):** G-buffer spec colour is sRGB-encoded (decoded softenLightF.glsl:221-223):
  `spec' = l2s(s2l(spec)·s)`, `env' = env·s`. Legacy diffuse has no `(1−F)` coupling (softenLightF.glsl:237-253).
- **Brightness:** linear albedo × k; `k > 1`: `k_eff = max(1, min(k, 1/max(maxRGB, 1e-4)))` (maxRGB .8 ⇒ 1.25).
  What changes: the diffuse lobe (and metal F0, which is base colour) × k_eff. Dielectric specular (fixed F0 0.04)
  and emission are NOT scaled: −1 EV is exactly −1 stop only where diffuse dominates (use Tame/Glow for the rest).
  Legacy albedo is sRGB-encoded (diffuseF.glsl:74-87 precedent) → decode, scale, re-encode. Unlit/fullbright and
  graphic-Cover presented colour: plain × k (HDR, no ceiling).
- **Glow:** creator-authored emissive colour and authored glow × g. Actor FX synthetic (look) emission never scaled.
- Every transform is skipped when `alrEnabled == 0`; each sub-transform is skipped at its own identity (`d == 0`,
  `s == 1`, `k == 1`, `g == 1`) — no sRGB round trips for untouched controls.

## 2. Architecture

### 2.1 Carrier: Actor FX programs + draw ownership, with an independent uniform pair
- Every avatar-family material program already has `hasActorFx` + `HAS_ACTOR_FX` (llviewershadermgr.cpp:1797-1998,
  2040-2140, 2195-2371, 2813/2822, 2924-3204, 3608/3640, 5200-5313) and every such draw already calls a per-draw
  upload: pools `uploadActorFx(params)` (lldrawpool.cpp:745), bodies/eyes `uploadActorFx(actor_id)`
  (lldrawpoolavatar.cpp:561/893/1104/1147), shared replays (llactormover.cpp:17187/17487/17581), Ghost deferred
  proxies `uploadActorFxDisabled()` (pipeline.cpp:5562-6020).
- `LLDrawInfo::mActorFxOwner/mActorFxFallbackOwner` (llspatialpartition.h:118-119) is set for EVERY avatar-owned face
  in `registerFace` (llvovolume.cpp:5563-5605, 5674, 6017-6018); world = null; owner-aware batching
  (llvovolume.cpp:5942-5943) prevents cross-avatar merging.
- Uniform uploads are value-cached (llglslshader.cpp:1779-1794); cache cleared on relink (llglslshader.cpp:892).
- Separate uniforms (not `ActorStyle`): the style is cast-only and its upload early-outs whenever no look is active
  (lldrawpool.cpp:593-600, 728-734, 759-762).
- Rejected: screen-space pass on the 1-bit avatar tag (llshadermgr.cpp:700-718); a new permutation.

### 2.2 PBR G-buffer carrier for Tame
Deferred PBR stores base colour + ORM only. Normal `.b` (gbufferUtil.glsl:54) is written as exactly 0 by every
PBR-flag writer (pbropaqueF.glsl:204/206, pbropaqueIndexedF.glsl:317/319, pbrterrainF.glsl:433, occlusionF.glsl:33)
and read only by LEGACY branches (softenLightF.glsl:231/285-287, spotLightF.glsl:228-306). Avatar PBR pixels write
`.b = t`; PBR branches of the four deferred light shaders use `s = 1 − .b`. Formats RGBA16 / RGB10_A2
(pipeline.cpp:614-623). ReShade provider uses `.xy`/`.w` only (reshade-addon/shaders/SL_GBufferProvider.fx:240-243).
`screenSpaceReflPostF.glsl` is never bound.

### 2.3 Impostors and update rate (R2 P1-A)
`impostorF.glsl:59` replays bakes with the LEGACY flag and raw `.b` as envIntensity; alpha-impostor programs
(llviewershadermgr.cpp:2855) have no Actor FX link. So adjusted avatars are kept live instead:
- llvoavatar.cpp `computeUpdatePeriod()` (:4630), directly after the Actor FX early-out block (:4633-4637):
  ```cpp
  if (!isVisuallyMuted() && ALAvatarLightResponse::keepsLive(this)) { mUpdatePeriod = 1; return; }
  ```
  `keepsLive(av)` = `sEverActive` && the effective slot (§3 keys; control avatar: own key then wearer) is present and
  non-identity, via non-counting `peek()`. Ghost avatars already return at :4633. `isImpostor()` (:12399-12420)
  returns `visually_muted || (sLimitNonImpostors && mUpdatePeriod > 1)` and the pool/pipeline impostor shortcuts
  (lldrawpoolavatar.cpp:512-513, 1021-1022; pipeline.cpp:4368-4376) call it ⇒ full geometry AND per-frame animation,
  with no `isImpostor` edit. Muted / Never Render / too-complex / too-slow avatars stay jellydoll/impostor and
  untrimmed (the trim is not a reason to override the user's performance or mute policy).
- Budget consequence: every adjusted, visible, non-muted avatar renders and animates at full rate outside
  `sMaxNonImpostors`. No cap (the user chose each one); the floater shows an amber warning (§8.1).
- During `LLPipeline::sImpostorRender` uploads write identity, so no bake ever carries a trim or carrier. Each change
  dirties the target's impostor (§4.1) so an avatar leaving "adjusted" rebakes clean.

## 3. Target identity (keys)

| Target | Store key | Draw-time keys, in order (first PRESENT slot wins; identity terminates) |
|---|---|---|
| Resident / self | agent UUID (`gAgentID` for self; serialized `"self"`) | `mActorFxOwner` (llvovolume.cpp:5603); body path `getActorFxOwnerId()` (llvoavatar.cpp:12356) |
| Animesh (standalone or worn) | control avatar `getActorFxOwnerId()` = linkset root (== `canonicalActorId`, lldirectorcast.cpp:1021-1043) | `mActorFxOwner`, then `mActorFxFallbackOwner` (wearer) |
| Ghost Studio clone | `ALGhostStudio::Instance::mId` (alghoststudio.h:353) | proxies/overlays: instance id, batch owner, batch fallback. Entity clones (`BACKING_ENTITY_CLONE`): table maps `mEntityId` → instance slot, else source slot (`mSource`, null ⇒ self) |

Director null-is-You (lldrawpool.h:367-370) is mapped to `gAgentID` only at the UUID-overload call site; draw-info
null = "not an actor" (identity). Clone keys are session-only.

## 4. New files

1. `indra/newview/alavatarlightresponsemodel.{h,cpp}` — pure (LLSD + math), unit-tested:
   ```cpp
   namespace ALLightResponse {
   struct Params { F32 mDiffuse = 0.f, mTame = 0.f, mExposureEV = 0.f, mGlow = 1.f; bool mBypass = false; };
   Params    sanitize(const Params&);
   bool      isIdentity(const Params&);             // bypass || all within 1e-4 of default
   LLVector4 pack(const Params&);                   // (d, 1-t, exp2(e), g)
   F32       albedoGain(F32 k, F32 max_rgb);        // CPU mirror of alr_gain
   F32       legacyGloss(F32 gloss, F32 d);         // CPU mirror of the §1.2 rule
   LLVector3 graphicCover(LLVector3 c, LLVector3 lit_pre, F32 sigma, F32 k);   // CPU mirror of §7.3
   enum EPreset { RESET, POWDERED, MATTE_SKIN, TAME_GLOSS, KILL_SHEEN, MINUS_HALF_STOP, MINUS_ONE_STOP, GLOW_OFF, PRESET_COUNT };
   Params    applyPreset(const Params& cur, EPreset);
   LLSD      toLLSD(const Params&);  Params fromLLSD(const LLSD&);  // "diffuse","tame","ev","glow","bypass"
   }
   ```
2. `indra/newview/alavatarlightresponse.{h,cpp}` — `class ALAvatarLightResponse`, `static ALAvatarLightResponse&
   instance()` returning a function-local static (idiom of `ALGhostStudio::instance()`, alghoststudio.h:76; not
   LLSingleton — per-draw path) and public `static bool sEverActive` (checked before `instance()`).
   - `enum EKind { KIND_RESIDENT, KIND_SELF, KIND_ANIMESH, KIND_CLONE }`; `struct Entry { EKind mKind; std::string mLabel; Params mParams; }`;
     `std::map<LLUUID, Entry> mEntries`. Presence is meaningful: present = explicit (identity and bypassed entries
     are kept and stop fallback); absent = inherit.
   - `static LLUUID keyForAvatar(const LLVOAvatar*, EKind* out)` — self → gAgentID; `isGhostAvatar()` → Studio instance
     with `mEntityId == av->getID()` → `mId`; control avatar → `getActorFxOwnerId()`; else `getID()`.
   - `set(key, kind, label, Params)` (sanitizes; keeps identity), `inherit(key)` (erase), `clearAll()`. Any
     non-identity value set or loaded ⇒ `sEverActive = true` (sticky). Each mutation: `mDirty`, change signal,
     impostor dirtying (§4.1), save (§8.3).
   - Table `std::unordered_map<LLUUID, Slot>`, `Slot { bool mIdentity; LLVector4 mPacked; }`, rebuilt lazily when
     `mDirty || mTableFrame != gFrameCount` (llappviewer.h:398); empty if master off. Every entry → slot; then each
     Studio `BACKING_ENTITY_CLONE` instance with non-null `mEntityId`: `slot(mId)` if present else `slot(mSource or gAgentID)` if present.
   - `const LLVector4* lookup(k1, k2, k3, LLUUID* hit_key)`: first non-null key with a slot terminates (identity →
     nullptr). `peek(...)` = lookup without side effects. `noteUpload(hit_key)` / `noteVisible(key)` counters (§10),
     swapped into `*LastFrame` on rebuild.
   - `loadFromFile()/saveToFile()` — `LL_PATH_PER_SL_ACCOUNT` `avatar_light_response.xml`, LLSD
     `{version:1, entries:{<uuid|"self">:{kind,label,diffuse,tame,ev,glow,bypass}}}`; clones never written.
   - `LLSD exportFor(ids)` / `importMerge(LLSD)` (Director scenes, §8.4).
3. `indra/newview/alfloateravatarlightresponse.{h,cpp}` + `skins/default/xui/en/floater_avatar_light_response.xml` (§8.1).
4. `indra/newview/app_settings/shaders/class1/alchemy/avatarLightResponseF.glsl` (§6).
5. `indra/newview/tests/alavatarlightresponsemodel_test.cpp` (§12.1).

CMake: add the pairs to `indra/newview/CMakeLists.txt` source/header lists (beside `alcinelightrigmodel` :292/:1151)
and the model to `viewer_TEST_SOURCE_FILES` (:2609-2616).

### 4.1 Impostor dirtying
On set/inherit/clear/master toggle: resolve the key (`gObjectList.findObject`; avatar → itself; object →
`getControlAvatar()`; clone → `ALGhostStudio::resolveEntityClone`) and set `mNeedsImpostorUpdate = true`
(llvoavatar.h:710). Master toggle / clear-all: `LLVOAvatar::resetImpostors()` (llvoavatar.h:706).

## 5. CPU upload hook

### 5.1 API (lldrawpool.h, beside :378-383)
```cpp
enum EAlrSlot { ALR_SLOT_BEAUTY = 0, ALR_SLOT_GLOW = 1 };   // GLOW: pack z := g (actorghost SWEEP_GLOW only)
static bool uploadActorFxStyleOnly(const LLUUID& actor_id, bool allow_native_wire = false, bool force_native = false);
static void uploadAvatarLightResponse(const LLUUID& k1, const LLUUID& k2 = LLUUID::null, const LLUUID& k3 = LLUUID::null);
static void uploadAvatarLightResponseTo(LLGLSLShader& shader, const LLUUID& k1, const LLUUID& k2, const LLUUID& k3,
                                        EAlrSlot slot = ALR_SLOT_BEAUTY);   // actorghost programs (no hasActorFx)
static void uploadAvatarLightResponseDisabled();
```
### 5.2 Implementation (lldrawpool.cpp anon namespace :433-457)
```cpp
const LLStaticHashedString sAlrEnabled("alrEnabled");
const LLStaticHashedString sAlrParams("alrParams");
void upload_alr(LLGLSLShader& shader, const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, EAlrSlot slot)
{
    static LLCachedControl<bool> debug_tint(gSavedSettings, "AvatarLightResponseDebugTint", false);
    LLUUID hit;
    const LLVector4* p = (LLPipeline::sImpostorRender || (k1.isNull() && k2.isNull() && k3.isNull()))
        ? nullptr : ALAvatarLightResponse::instance().lookup(k1, k2, k3, &hit);
    if (!p) { shader.uniform1i(sAlrEnabled, 0); return; }
    LLVector4 v = *p;
    if (slot == ALR_SLOT_GLOW) v.mV[VZ] = v.mV[VW];
    shader.uniform4fv(sAlrParams, 1, v.mV);
    shader.uniform1i(sAlrEnabled, debug_tint() ? 3 : 1);
    if (shader.getUniformLocation(sAlrParams) >= 0)             // count only real consumers
        ALAvatarLightResponse::instance().noteUpload(hit);
}
bool LLRenderPass::uploadActorFxStyleOnly(const LLUUID& id, bool wire, bool force)
{ return upload_actor_fx_for_id(id, wire, force); }            // = today's uploadActorFx(UUID) body, moved verbatim
void LLRenderPass::uploadAvatarLightResponse(const LLUUID& k1, const LLUUID& k2, const LLUUID& k3)
{
    if (!ALAvatarLightResponse::sEverActive) return;          // fresh session: zero added GL calls
    LLGLSLShader* s = LLGLSLShader::sCurBoundShaderPtr;
    if (s && s->mFeatures.hasActorFx) upload_alr(*s, k1, k2, k3, ALR_SLOT_BEAUTY);
}
void LLRenderPass::uploadAvatarLightResponseTo(LLGLSLShader& s, const LLUUID& k1, const LLUUID& k2, const LLUUID& k3, EAlrSlot slot)
{ if (ALAvatarLightResponse::sEverActive) upload_alr(s, k1, k2, k3, slot); }
void LLRenderPass::uploadAvatarLightResponseDisabled()
{
    if (!ALAvatarLightResponse::sEverActive) return;
    LLGLSLShader* s = LLGLSLShader::sCurBoundShaderPtr;
    if (s && s->mFeatures.hasActorFx) s->uniform1i(sAlrEnabled, 0);
}
```
### 5.3 Generic call sites (response uploaded LAST, so an internal Actor FX "disabled" cannot clobber it)
1. `uploadActorFxDisabled()` (lldrawpool.cpp:460-469): append `uploadAvatarLightResponseDisabled();`.
2. `uploadActorFx(const LLUUID&, …)` (:724-742): body moved verbatim to file-static `upload_actor_fx_for_id`; new
   body `bool r = uploadActorFxStyleOnly(actor_id, wire, force); uploadAvatarLightResponse(actor_id.isNull() ? gAgentID : actor_id); return r;`
3. `uploadActorFx(const LLDrawInfo&, …)` (:745-765): same wrap; then `uploadAvatarLightResponse(params.mActorFxOwner, params.mActorFxFallbackOwner);`
### 5.4 Avatar pool (owned draws that deliberately disable Actor FX)
lldrawpoolavatar.cpp:1098-1101 and :1141-1144 (`shared_replay` → `uploadActorFxDisabled()` for an OWNED body): add
`LLRenderPass::uploadAvatarLightResponse(actor_fx_id);` right after. :1125/:1197 unchanged.
### 5.5 Shared Actor FX replay — llactormover.cpp (R1 P1-4, R2 P2)
Replace `LLRenderPass::uploadActorFx(proxy.mStyleId, …, true)` at :17187, :17487, :17581 with
`LLRenderPass::uploadActorFxStyleOnly(proxy.mStyleId, …, true);` followed by exactly one
`LLRenderPass::uploadAvatarLightResponse(o->mActorFxOwner, o->mActorFxFallbackOwner);` where `o` is the command's
own draw info — `policy_info` at :17187 (declared :17144-17145), `info` at :17487, `face->mDrawInfo` at :17581; if
`o` is null use `uploadAvatarLightResponse(proxy.mStyleId.isNull() ? gAgentID : proxy.mStyleId)`. Covers beauty and
authored-glow commands (`command.mAuthoredGlow`, :17140-17141). The `uploadActorFxDisabled()` resets at :17524/:17603
stay (they also zero the response, correct at teardown).
### 5.6 Ghost clones — pipeline.cpp / llactormover.cpp
- Deferred proxies (pipeline.cpp): file-static `LLUUID sGhostAlrInstance` + RAII setter right after each
  `ScopedGhostTransform transform(proxy);` (:6122, :6704, :7046) = `proxy.mInstanceId`. After each
  `LLRenderPass::uploadActorFxDisabled();` in pushGhostBatch :5562, pushGhostGLTFBatch :5608, pushGhostMaterialBatch
  :5696, pushGhostGLTFBatchIndexed :5819, pushGhostBumpBatch :5952, pushGhostFullbrightShinyBatch :6020:
  `LLRenderPass::uploadAvatarLightResponse(sGhostAlrInstance, params.mActorFxOwner, params.mActorFxFallbackOwner);`.
- Overlay sweeps: add `LLUUID mLightResponseKey;` to `LLActorMover::GhostDrawParams` (llactormover.h:989); set
  `= inst.mId` where Studio instances build `gp` (llactormover.cpp:14765-14771) and `= proxy.mInstanceId` at the
  proxy draws (:17792, :17811). Path ghosts and shared Actor FX live replays (`gp.mWorldLinear`, :11749-11750) leave
  it null. In `drawGeometryGhost` (:11714), for EVERY batch and static face of EVERY style, upload to the program
  that is BOUND for that draw — never to the outer `shader`/`fx` variable:
  - in `draw_batches`: to `*batch_shader`, after `apply_program(batch_shader)` (:12034-12037) and after its other
    per-draw uniforms (next to the `sGhostAux`/`sGhostSlot` uploads, :12230-12247), before the draw call;
  - in `draw_static`: to `*static_shader`, after `apply_program(static_shader)` (:12499) and its per-face uniforms
    (next to :12590-12603), before the draw call (static faces never run `SWEEP_GLOW`, :12495);
  - any other draw inside `drawGeometryGhost` that uploads `sGhostAux` to some program (e.g. :12414): upload to that
    same program object at that same point.
  `LLGLSLShader::uniform*` resolves the location and value cache from the object it is called on while `glUniform*`
  writes the currently bound program, so object and binding MUST match. Code at each site (`prog` = that program):
  ```cpp
  const bool alr_on = style == GHOST_STYLE_CLONE && gp.mLightResponseKey.notNull();
  LLRenderPass::uploadAvatarLightResponseTo(*prog,
      alr_on ? gp.mLightResponseKey : LLUUID::null,
      alr_on && di ? di->mActorFxOwner : LLUUID::null,
      alr_on && di ? di->mActorFxFallbackOwner : LLUUID::null,
      subset == SWEEP_GLOW ? LLRenderPass::ALR_SLOT_GLOW : LLRenderPass::ALR_SLOT_BEAUTY);
  ```
  (`di` = `batch.mInfo`, or `face->mDrawInfo` for static faces; `subset` is the sweep enum, :11972). Non-CLONE
  styles and Actor FX replays therefore always get identity — no leakage between styles or programs. Any other draw
  that binds `gActorGhostProgram` / `gWorldActorGhostProgram` outside `drawGeometryGhost` must upload identity once
  after bind (reviewer: enumerate them; `gAvatarActorGhostProgram` / `gAvatarEyeballActorGhostProgram`, :15086-15185,
  never receive a non-zero value so they keep the link default).

## 6. GLSL module `class1/alchemy/avatarLightResponseF.glsl`

Registration: basic fragment list after actorFxF.glsl (llviewershadermgr.cpp:1328), CORE (fatal and loud; ~80 lines,
no dependencies). Attach in `LLShaderMgr::attachShaderFeatures` inside `if (features->hasActorFx)`
(llshadermgr.cpp:218-227) after the beauty module. Never attached for `hasActorFxShadow`-only programs.

```glsl
// [AvatarLightResponse] Identity when alrEnabled == 0 (GL link default).
uniform int  alrEnabled;   // 0 identity; 1 active; 3 active + debug override
uniform vec4 alrParams;    // x diffuse d, y specular keep s, z albedo gain k, w authored-emission gain g
const vec3 ALR_MAGENTA = vec3(1.0, 0.0, 1.0);
float alr_s2l(float c) { c = max(c, 0.0); return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); }
float alr_l2s(float c) { c = max(c, 0.0); return c <= 0.0031308 ? c * 12.92 : 1.055 * pow(c, 1.0 / 2.4) - 0.055; }
vec3 alr_s2l3(vec3 c) { return vec3(alr_s2l(c.r), alr_s2l(c.g), alr_s2l(c.b)); }
vec3 alr_l2s3(vec3 c) { return vec3(alr_l2s(c.r), alr_l2s(c.g), alr_l2s(c.b)); }
vec3 alr_gain(vec3 c, float k)
{
    if (k > 1.0) { float m = max(max(c.r, c.g), c.b); k = max(1.0, min(k, 1.0 / max(m, 1e-4))); }
    return c * k;
}
vec3 alrAlbedoLinear(vec3 c)
{
    if (alrEnabled == 0) return c;
    if (alrEnabled == 3) return ALR_MAGENTA;
    return alrParams.z == 1.0 ? c : alr_gain(c, alrParams.z);
}
vec3 alrAlbedoSrgb(vec3 c)
{
    if (alrEnabled == 0 || (alrEnabled == 1 && alrParams.z == 1.0)) return c;   // no sRGB round trip at identity
    return alrEnabled == 3 ? ALR_MAGENTA : alr_l2s3(alr_gain(alr_s2l3(c), alrParams.z));
}
vec3 alrPresentedLinear(vec3 c)                       // unlit / fullbright output (HDR, no ceiling)
{
    if (alrEnabled == 0) return c;
    return alrEnabled == 3 ? ALR_MAGENTA : c * alrParams.z;
}
vec3 alrGraphicCover(vec3 c, vec3 lit_pre, float sigma)   // §7.3: trims only the fx term of mix(lit_pre, fx, sigma)
{
    if (alrEnabled == 0 || sigma <= 0.0) return c;
    if (alrEnabled == 3) return ALR_MAGENTA;
    return alrParams.z == 1.0 ? c : c + (alrParams.z - 1.0) * (c - (1.0 - sigma) * lit_pre);
}
float alrRoughness(float r)
{
    if (alrEnabled == 0) return r;
    return alrEnabled == 3 ? 1.0 : mix(r, 1.0, clamp(alrParams.x, 0.0, 1.0));
}
void alrLegacySpec(inout vec3 spec_srgb, inout float gloss, inout float env)
{
    if (alrEnabled == 0) return;
    if (alrEnabled == 3) { spec_srgb = vec3(0.0); env = 0.0; return; }
    float d = clamp(alrParams.x, 0.0, 1.0);
    if (d > 0.0 && gloss >= 0.5 / 255.0) gloss = max(gloss * (1.0 - 0.95 * d), 1.0 / 255.0);
    if (alrParams.y != 1.0) { spec_srgb = alr_l2s3(alr_s2l3(spec_srgb) * alrParams.y); env *= alrParams.y; }
}
float alrSpecKeep()       { return alrEnabled == 0 ? 1.0 : (alrEnabled == 3 ? 0.0 : alrParams.y); }
vec3  alrEmissive(vec3 e) { return alrEnabled == 0 ? e : e * alrParams.w; }
float alrEmissiveGain()   { return alrEnabled == 0 ? 1.0 : alrParams.w; }
float alrPbrCarrier()     { return alrEnabled == 0 ? 0.0 : 1.0 - alrSpecKeep(); }
vec3  alrDebugEmission(vec3 e) { return alrEnabled == 3 ? ALR_MAGENTA * 4.0 : e; }   // REPLACES emission in debug
bool  alrDebug()          { return alrEnabled == 3; }
```
Own sRGB helpers: not every `hasActorFx` program links srgbF.glsl (e.g. gAvatarProgram). Consumers declare only the
prototypes they call, inside `#ifdef HAS_ACTOR_FX`.

## 7. Shader edits — every beauty path an avatar pixel can take

Order rule: albedo / roughness / legacy spec act on the PRESENTED surface (after the Actor FX block, right before
the G-buffer write or lighting). Glow acts on AUTHORED emission before `actorFxEmissive`. All inserts in
`#ifdef HAS_ACTOR_FX` unless stated. **Debug emission (all deferred writers):** as the LAST statement of each writer's
`#if defined(HAS_EMISSIVE)` block (after any Actor FX emissive assignment) add
`frag_data[3].rgb = alrDebugEmission(frag_data[3].rgb);` — in debug this replaces authored and synthetic emission.

### 7.1 G-buffer writers (deferred opaque / masked)
| File | Insert |
|---|---|
| class1/deferred/pbropaqueF.glsl | first line inside the block opened at :185: `emissive = alrEmissive(emissive);`; before its `#endif` :191: `col = alrAlbedoLinear(col); spec.g = alrRoughness(spec.g);`; `float alr_env_carrier = 0.0;` before :200 (`= alrPbrCarrier();` under ifdef), passed instead of `0` at :204/:206 |
| class1/deferred/pbropaqueIndexedF.glsl | same: block :304-:310, writes :317/:319 |
| class3/deferred/materialF.glsl | before :423: `diffcol.rgb = alrAlbedoSrgb(diffcol.rgb); alrLegacySpec(spec.rgb, glossiness, env);` (deferred and forward BLEND; BLEND debug override in §7.2) |
| class1/deferred/materialIndexedF.glsl | same two lines before :353 |
| class1/deferred/bumpF.glsl | before :112, unconditionally `vec4 alr_spec = vertex_color.aaaa * actor_fx_material_response; float alr_env = vertex_color.a * actor_fx_material_response;` (bit-identical to today); ifdef'd `col.rgb = alrAlbedoSrgb(col.rgb); alrLegacySpec(alr_spec.rgb, alr_spec.a, alr_env);`; write `frag_data[1] = alr_spec`; pass `alr_env` at :117/:120 |
| class1/deferred/diffuseF.glsl | same pattern before :91 (writes :92, :95/:99); also the avatar-eyes program |
| class1/deferred/diffuseIndexedF.glsl | same pattern before :94 (`vec4(spec, vertex_color.a)` form, :95, :98/:102) |
| class1/deferred/diffuseAlphaMaskIndexedF.glsl / diffuseAlphaMaskNoColorF.glsl (rigid eyes, lldrawpoolavatar.cpp:754) | before :90 / :86: `col.rgb = alrAlbedoSrgb(col.rgb);` |
| class1/deferred/avatarF.glsl (system/BOM body) | before :90: `diff.rgb = alrAlbedoSrgb(diff.rgb);` (spec 0 ⇒ Brightness only) |

### 7.2 Forward paths (lit in-shader)
Debug override: immediately before each listed final `frag_color` write, `if (alrDebug()) <colour>.rgb = vec3(1.0, 0.0, 1.0);`
(`<colour>` = the variable that write uses: `color`).
| File | Insert |
|---|---|
| class2/deferred/pbralphaF.glsl | first line inside block :279 `colorEmissive = alrEmissive(colorEmissive);`; before `#endif` :285 `col = alrAlbedoLinear(col); perceptualRoughness = alrRoughness(perceptualRoughness);`; `float alr_keep = 1.0;` (= `alrSpecKeep()` under ifdef) before :300; :300 → `pbrBaseLightKeep(…, alr_keep)`; LIGHT_LOOP :307 → `pbrCalcPointLightOrSpotLightRimKeep(…, rig_rim_lights[i], alr_keep)`; prototypes beside :118/:146; debug before :325 |
| class2/deferred/sharedActorFxPbrF.glsl | block :513-:523 as pbralpha; :557 `pbrBaseLightKeep`; its LIGHT_LOOP (after :563) → RimKeep; prototypes beside :152/:184; graphic Cover §7.3; debug before :605 |
| class2/deferred/alphaF.glsl (legacy alpha, IS_AVATAR_SKIN) | after :321: `diffuse_linear.rgb = alrAlbedoLinear(diffuse_linear.rgb);`; debug before :398 |
| class1/deferred/fullbrightF.glsl | before :156: `color.rgb = alrPresentedLinear(color.rgb);` (inside `#ifndef IS_HUD`); debug before :171 |
| class3/deferred/fullbrightShinyF.glsl | at :144 (after the actor_fx_emissive block, before applyLegacyEnv :145): `color.rgb = alrPresentedLinear(color.rgb); env_intensity *= alrSpecKeep();`; debug before :160 |
| class3/deferred/materialF.glsl BLEND variant | debug before :552 |

### 7.3 Actor FX shared replay specifics
- class1/deferred/sharedActorFxPbrGlowF.glsl: after :297 (`float authored_lum = …;`) `authored_lum *= alrEmissiveGain();`.
  `synthetic_lum` untouched; sharedActorFxPbrSyntheticGlowF.glsl untouched (look-only).
- **Graphic Cover (R2 P1-C).** actorFxF.glsl, after `actorFxActive()` (:513-516) — all dependencies precede it
  (`actorFxCoverMode` :478, `actorFxFlatSensorLook` :488, `actorFxPbrStrength` :505):
  ```glsl
  float actorFxGraphicCoverStrength()   // σ of the graphic-Cover mix at :1948; 0 when not graphic Cover
  {
      return (actorFxActive() && actorFxCoverMode() && (actorFxFlatSensorLook() || actorFxLook == 11))
          ? actorFxPbrStrength() : 0.0;   // predicate identical to :1526-1527
  }
  ```
  actorFxFallbackF.glsl: `float actorFxGraphicCoverStrength() { return 0.0; }`. sharedActorFxPbrF: declare the
  prototype with the other Actor FX prototypes (:117-134); `vec3 alr_lit_pre = color;` immediately before the
  `actorFxPbrPostLight(...)` call (:584); right after the call (:588) and BEFORE `color += actor_fx_authored_emission
  + actor_fx_synthetic_emission;` (:591): `color = alrGraphicCover(color, alr_lit_pre, actorFxGraphicCoverStrength());`.
  Since `actorFxPbrPostLight` returns `mix(layer_source, fx, σ)` with `layer_source == lit_color` (:1489, :1948, its
  only non-early return), the result is `(1−σ)·lit_pre + σ·k·fx`: the lit part keeps its single pre-light trim, the
  graphic part gets k once, synthetic emission (added after) is untouched. Layer/physical looks: σ = 0 ⇒ unchanged.
- class1/deferred/emissiveF.glsl / emissiveIndexedF.glsl: after `float a = diffuse_alpha * vertex_color.a;` (:78 / :159)
  `a *= alrEmissiveGain();` — before the synthetic add.
- class1/deferred/pbrglowF.glsl / pbrglowIndexedF.glsl: after the emissive texel multiply (:114 / :187, before
  `float lum`): `emissive = alrEmissive(emissive);`.

### 7.4 Deferred light shaders and deferredUtil helpers (R1 P1-1)
- deferredUtil.glsl: body of `pbrBaseLight` (:1035-1086) becomes `vec3 pbrBaseLightKeep(…same params…, float spec_keep)`
  with `iblSpec *= spec_keep;` after `pbrIbl(...)` (:1043) and `specPunc *= spec_keep;` after `pbrPunctual(...)`
  (:1051) (feeds classic :1072 and normal :1078). `pbrBaseLight(...)` = `return pbrBaseLightKeep(..., 1.0);`. Same for
  `pbrCalcPointLightOrSpotLightRim` (:928-984) → `...RimKeep(..., vec4 rim, float spec_keep)`, `specPunc *= spec_keep;`
  after :964; the old Rim name forwards 1.0 (the rimless wrapper :986-1002 is unchanged). `x * 1.0` is exact; waterF
  only declares `pbrBaseLight`.
- softenLightF.glsl :201 → `pbrBaseLightKeep(…, 1.0 - gb.envIntensity)`; prototype beside :90.
- multiPointLightF.glsl after :129: `specPunc *= 1.0 - gb.envIntensity;` (inside the loop, before :130).
- pointLightF.glsl after :123; spotLightF.glsl after both `pbrPunctual` calls (:203, :209).

### 7.5 Ghost overlay CLONE sweeps (R2 P1-B) — class1/interface/actorghostF.glsl
Declare locally `uniform int alrEnabled; uniform vec4 alrParams;` (actorghost programs do not link the module) and a
local `float alr_ghost_l2s(float)` (inverse of the existing `ghostSrgbChannelToLinear`). Insert AFTER the
`if (ghostWorldLinear != 0) { … }` block (:1457-1471) and BEFORE `#ifdef GHOST_WORLD_PASS` (:1472) — i.e. in both the
world and the late display-space overlay compilations:
```glsl
if (alrEnabled == 1 && ghostLook == 1)          // GHOST_STYLE_CLONE; CPU only sends non-zero for Studio CLONE draws
{
    if (alrParams.z != 1.0)
        rgb = ghostWorldLinear != 0 ? rgb * alrParams.z   // scene-linear
            : vec3(alr_ghost_l2s(ghostSrgbChannelToLinear(rgb.r) * alrParams.z),
                   alr_ghost_l2s(ghostSrgbChannelToLinear(rgb.g) * alrParams.z),
                   alr_ghost_l2s(ghostSrgbChannelToLinear(rgb.b) * alrParams.z));   // display-space overlay: scale light, not code values
}
```
Debug override (final, R3): immediately before the final beauty write `frag_color = max(vec4(rgb, alpha), vec4(0));`
(:1539) — after signal modulation (:1477-1478), additive radiance (:1487) and fog (:1515-1537) — add
`if (alrEnabled == 3 && ghostLook == 1) rgb = vec3(1.0, 0.0, 1.0);`. The glow-only early return (:1511) is untouched.
Beauty sweeps (SOLID/BLEND) arrive with z = k (Brightness); `SWEEP_GLOW` arrives with z = g (Glow) via
`ALR_SLOT_GLOW` — the additive authored-emissive sweep, which therefore never receives Brightness. `ghostGlowOnly`
(:1488-1512, synthetic `worldRadiance`) is not touched. Diffuse/Tame: N/A (unlit copy). Actor FX Cover looks
replayed for legacy/system bodies through actorghost programs get identity by design (the look owns the output);
only PBR graphic Cover gets Brightness (§7.3) — asymmetry documented.

### 7.6 Not edited
Shadow/velocity/depth shaders; impostorF.glsl and the alpha-impostor programs (adjusted avatars never bake, §2.3);
actorFxGlowF/actorFxPbrGlowF and sharedActorFxPbrSyntheticGlowF (look-only); HUD variants; water, terrain, sky;
screenSpaceReflPostF.glsl (unbound).

## 8. Settings, UI, persistence

### 8.1 Floater `avatar_light_response` (`ALFloaterAvatarLightResponse`, ~360×470)
Register beside ghost_studio (llviewerfloaterreg.cpp:779). `onOpen(key)`: select/create the row for a UUID key.
- `chk_enabled` → `AvatarLightResponseEnabled`.
- `target_list` (multi_select): columns `state` (● green = uploaded AND visible last frame; ● amber = adjusted but not
  uploaded, not visible, or muted/jellydolled (§2.3); ○ = explicit identity/bypass; blank = inherit), `name`, `kind`.
  Rows: You; Director cast; residents in draw distance (`LLWorld::getAvatars`, llworld.h:181); animesh in view
  (`LLCharacter::sInstances`, `isControlAvatar() && !isDead()`, "Animesh <8-hex>"); Studio instances
  (`mName`/`mSourceLabel`); stored entries not present ("(away)"). Names via `LLAvatarNameCache::get`.
  `chk_only_adjusted`, `btn_refresh`; 1 Hz timer refreshes `state`.
- Edit group (all selected rows): `sld_diffuse`, `sld_tame`, `sld_brightness` ("+0.00 EV"), `sld_glow`,
  `chk_bypass`. Live on commit; save on slider mouse-up (`setSliderMouseUpCallback`, llsliderctrl.h:131).
- `combo_preset` + `btn_apply_preset`; `btn_reset_selected` (explicit identity); `btn_inherit_selected` (remove;
  enabled only for animesh/clone rows); `btn_reset_all` (confirm, new notification `AvatarLightResponseResetAll`);
  `btn_copy` / `btn_paste`; `chk_debug_tint` → `AvatarLightResponseDebugTint`.
- Status: "N adjusted · U uploaded · V visible". Amber budget line when (adjusted, visible, non-muted) >
  `LLVOAvatar::sMaxNonImpostors` and `sLimitNonImpostors`: "K adjusted avatars render at full detail beyond your
  avatar impostor limit (M) — expect lower FPS." Tooltip: "Changes how this avatar responds to every light, on your
  screen only. Adjusted avatars are always rendered and animated in full."

### 8.2 Entry points
- Context menus: inside `<context_menu name="Director">`, after `ActorMover Target`: `menu_item_call label="Light
  Response..." name="Light Response"`, `on_click function="Avatar.LightResponse"`, `on_enable
  function="Avatar.EnableAlphaMode"` — menu_avatar_other.xml:185, menu_attachment_other.xml:77,
  menu_avatar_self.xml:370, menu_attachment_self.xml:76; menu_object.xml:115 with `on_visible
  function="Object.EnablePlayLocalAnim"` like its neighbours.
- alviewermenu.cpp: `handle_avatar_light_response` beside :988-1000, registered beside :1233:
  `find_avatar_from_object(primary)` → `keyForAvatar` → `LLFloaterReg::showInstance("avatar_light_response", LLSD(key))`.
- menu_viewer.xml machinima block after Ghost Studio (:141-147): "Avatar Light Response..." → `Floater.Toggle`.
- Director cast row menu (menu_director_cast.xml near :73-79) → `registrar.add("Director.LightResponse", …)` beside
  llfloaterdirector.cpp:373; `firstSelectedCastId()` (null ⇒ gAgentID) as in `onCastCopyUUID` (:1757-1767).

### 8.3 Settings / persistence
`AvatarLightResponseEnabled` Boolean 1 (persist) · `AvatarLightResponsePersist` Boolean 1 (persist) ·
`AvatarLightResponseDebugLog` Boolean 0 · `AvatarLightResponseDebugTint` Boolean 0 (both non-persistent).
Load at llstartup.cpp:1203 right after `LLRenderMuteList::getInstance()->loadFromFile();`. Save on mouse-up / preset /
reset / inherit / bypass and on floater close, when Persist is on. Explicit identity entries ARE saved.

### 8.4 Director scenes
`sceneData()` (lldirectorcast.cpp:2159, before `return data;`): `data["light_response"] = exportFor(cast ids, null ⇒ "self")`.
`applySceneData()` (before its closing brace :2406): `if (data.has("light_response")) importMerge(...)`.

## 9. Interactions and precise effects

### 9.1 Other systems
- Actor FX / Tron: §7 order. Layer looks keep the trimmed presented surface; PBR graphic Cover: Brightness on the
  graphic term only (§7.3); legacy Cover replays: identity (§7.5); look emission never glow-trimmed.
- Env Light Intensity v2: scene scales act inside probe sampling / irradiance (softenLightF.glsl:191-194); Tame
  scales evaluated IBL specular after them.
- Rig Rim (`rigRimTerm`) uses diffuseColor + roughness → follows Brightness and Diffuse; not Tame.
- RTGI / ReShade: published albedo, ORM and visible-diffuse sidecar carry Brightness/Diffuse; what the add-on does
  with them is outside this repo's proof ("albedo input is trimmed", not "bounce × k"). Tame invisible to the add-on.
- Auto-exposure (luminanceF.glsl) sees trimmed albedo. Mirrors/probes/tiled snapshots: same per-draw path.
  Shadows: unaffected. Performance: §2.3 budget note.

### 9.1b Legacy limitations (post-review)
- **Deferred legacy env reflection is not tamed.** For legacy (non-PBR) pixels the G-buffer holds ONE env
  value, and softenLightF uses it both as reflection magnitude and as the weight that fades the surface
  colour (1 - env). Scaling it would change diffuse; compensating albedo instead darkens local-light diffuse
  and is wrong in the class2 encoded-space mix. So deferred legacy writers keep albedo and env exactly as
  authored: Tame there = specular only (sun/local highlights, gloss reflection via the specular colour);
  Diffuse still softens gloss. An exact deferred env-reflection tame needs a new G-buffer channel.
- **Forward legacy env** (fullbrightShinyF, materialF BLEND) uses `applyLegacyEnvKeep`: mixing weight
  and Fresnel untouched, reflection addend x keep (identity bit-exact). The class2 probes-off variant scales
  the addend in ENCODED space (monotonic, not linear-exact).
- **Capacity:** at most 512 stored (non-clone) entries; `set()` refuses new ones past the cap (notification);
  load/import take "self" first, then explicit identity entries, then the rest; save never exceeds the cap.

### 9.2 Presets (`applyPreset`)
| Preset | Diffuse | Tame | EV | Glow |
|---|---|---|---|---|
| Reset | 0 | 0 | 0 | 100 |
| Powdered | 30 | 25 | keep | keep |
| Matte skin | 50 | 50 | keep | keep |
| Tame gloss | 15 | 65 | keep | keep |
| Kill sheen | 70 | 90 | keep | keep |
| −½ stop / −1 stop | keep | keep | −0.5 / −1.0 | keep |
| Glow off | keep | keep | keep | 0 |

## 10. Instrument (three separate facts)
1. **UPLOADED** — a program with an ACTIVE `alrParams` location received a non-identity upload for that key this
   frame (`noteUpload`, §5.2). Proves ownership resolution + a consumer program was bound; NOT a completed draw.
2. **VISIBLE** — `LLVOAvatar::isVisible()` for the resolved avatar (clone: proxy `mFrustumVisible` / entity visible);
   plus `muted` flag when `isVisuallyMuted()` (expected no-trim).
3. **CONSUMED** — only visual: DebugTint mode 3 overrides every covered path (§6/§7). Any non-magenta part of a
   tinted target is an uncovered path.
`AvatarLightResponseDebugLog` (1 Hz): `[ALR] targets=N explicit=E active=A live_over_budget=K` and per active target
`[ALR] <label> key=<uuid> uploads=<n> visible=<0|1> muted=<0|1> VERDICT=` `UPLOADED` (n>0 && visible),
`FAIL-NOT-UPLOADED` (visible, !muted, n==0: key mismatch §3), `EXPECTED-MUTED`, `INCONCLUSIVE-NOT-VISIBLE`.
Startup: `[ALR] module attached to <n> programs`.

## 11. Proof obligations (reviewer checks each; not asserted by this doc)
1. Fresh session, no entries: `sEverActive == false` ⇒ zero added GL calls; shaders take `alrEnabled == 0`; carriers
   write 0.0; Keep helpers multiply by exactly 1.0. Necessary, not sufficient: pixel identity only via T0 captures.
2. Transitions: avatar → world draw on the same program (null owner writes 0); set → reset → inherit; master off/on;
   relink (`mapUniforms` clears `mValue`, llglslshader.cpp:892); any fast-bind path that draws avatar geometry
   without §5; fallback transitions (bypass a clone with an adjusted source ⇒ untrimmed; inherit ⇒ source); style
   switches inside `drawGeometryGhost` (CLONE → hologram gets identity); update-period transitions (adjusted ⇒ 1,
   reset ⇒ normal throttling resumes, impostor rebakes clean).
3. Coverage: every hasActorFx or actorghost draw of avatar geometry passes through §5.
4. Binary cache: `LLGLSLShader::hash()` (llglslshader.cpp:2067-2135) hashes only a program's own `mShaderFiles`.
   Every program that CALLS new code has an edited main file (incl. every caller of the Keep helpers and of
   `actorFxGraphicCoverStrength`); link-only programs may keep stale binaries that do not call it. T0 clears once.
5. No global GL state changes (uniforms only).

## 12. Test plan

### 12.1 Unit (TUT, `tests/alavatarlightresponsemodel_test.cpp`)
Defaults identity; `pack(default) == (0,1,1,1)`; clamps / non-finite; bypass ⇒ identity; presets exact, "keep"
preserved; `albedoGain` (k ≤ 1 exact; maxRGB .8, k 2 ⇒ 1.25; ≥ 1 when k > 1); `legacyGloss` (d 0 ⇒ unchanged for
any gloss; .02, d 1 ⇒ 1/255; 0 ⇒ 0; 0.001 ⇒ 0.001); `graphicCover` (σ 0 ⇒ c; σ 1 ⇒ k·c; σ .5, lit L, fx F ⇒
.5L + .5kF within 1e-6); LLSD round-trip incl. missing keys and explicit identity entries.

### 12.2 In-world (user runs; expected outcomes stated in advance)
Setup: clear shader cache once; DebugLog on; fixed EEP (time frozen), no water/clouds in view, subjects on a frozen
pose, fixed camera.
- **T0 OFF path (controlled):** 3 PNG captures with the pre-change exe backup (noise floor), 3 with the new build (no
  entries this session). Expect new-vs-old within the old-vs-old floor. Then set and reset a response
  (sEverActive true) and capture: same expectation.
- **T1 Debug override** (emissive buffer on — default outside vintage): tint on, target A adjusted. Expect every
  surface of A solid magenta — body, rigged mesh, PBR + legacy, alpha hair/lashes, eyes, fullbright, in shadow too;
  nothing else magenta. Repeat: worn animesh, standalone animesh, entity clone, CLONE overlay clone, proxy clone with
  GhostDeferredEnable. Any non-magenta part = uncovered path; report which. Qualifications: alpha-BLENDED surfaces
  (hair, lashes, sheer cloth) show magenta mixed with what is behind them in proportion to their alpha; post-processing
  (tonemap, exposure, bloom, DoF, colour grade, ReShade) shifts the exact hue — judge "magenta-dominant", and disable
  ReShade/grading for this test. Vintage mode (no emissive buffer): deferred parts show magenta only where lit.
- **T2 Diffuse** 0→100 % on a shiny PBR body: highlights widen and dim, reflections blur; shadowed tone unchanged.
- **T3 Tame** 0→100 %: highlights, sheen and env reflections vanish under sun, probes and a local light; a matte
  patch's brightness is unchanged on lit AND shadowed sides (no diffuse lift).
- **T4 Legacy** skin/hair with spec maps: Diffuse broadens (at 100 % a faint broad sheen remains); Tame removes shine.
- **T5 Brightness −1 EV:** diffuse regions half as bright; highlights and glow keep their level; others unchanged.
- **T6 Glow 0 %:** authored glow/emissive gone (incl. a CLONE overlay clone's glowing eyes); an Actor FX look's glow
  unchanged; CLONE clone at −1 EV keeps its eye glow level.
- **T7 Scope:** only A adjusted; bypass toggles A/B; master off restores all.
- **T8 Fallbacks:** worn pet inherits wearer; own values ⇒ follows them; Reset ⇒ pet untrimmed while wearer trimmed;
  Inherit ⇒ follows wearer. Same for a clone vs its source.
- **T9 Looks:** Layer look + Matte skin ⇒ matte persists. Graphic Cover look at partial strength + −1 EV ⇒ only the
  graphic contribution darkens once (compare Cover 100 % vs 50 %: the 50 % frame is not darker than the linear mix).
- **T10 Live policy:** crowded sim, low impostor limit, A far away and adjusted ⇒ A fully rendered AND animating
  smoothly (no 1-in-32 frame stepping); Reset ⇒ A may impostor again, untrimmed. Mute A ⇒ jellydoll, log
  `EXPECTED-MUTED`. Adjust more avatars than the limit ⇒ amber budget warning appears.
- **T11 Persistence:** relog ⇒ resident/animesh entries (incl. explicit identity) restored; clones gone. Director
  scene save/load restores a cast member's values.
- **T12 Shadows:** A at −2 EV / Tame 100 % casts an identical shadow.
- **T13 Transitions:** with A adjusted (tint on), walk A past world objects sharing materials; nothing else turns magenta.
Triage: send `[ALR]` lines. FAIL-NOT-UPLOADED = key mismatch; UPLOADED but not magenta = shader path gap.

## 13. Implementation phases (one delivery, one build)
P1 model + tests; P2 store (explicit/inherit, table, peek, persistence, impostor dirtying, counters); P3 GLSL module
+ registration + §7.1-§7.3 + actorFx helper; P4 deferredUtil Keep helpers + §7.4; P5 CPU hooks §5.3-§5.6 +
`computeUpdatePeriod` §2.3 + actorghostF §7.5; P6 floater, menus, Director row menu, top menu, settings, scene I/O.
Review P1-P6 together, then build once.

## 14. OFF-LIMITS
Do not modify: `upload_actor_fx_style` and all Actor FX look/suppression policy (lldrawpool.cpp:579-937, except the
§5.3 wrappers and the `uploadActorFxStyleOnly` exposure); actorFxF.glsl / actorFxFallbackF.glsl EXCEPT adding
`actorFxGraphicCoverStrength()`; actorFxDissolve*.glsl; deferredUtil.glsl beyond the two Keep refactors (`pbrIbl`,
`pbrPunctual`, `calcDiffuseSpecular`, `rigRimTerm` bodies unchanged); reflectionProbeF.glsl; impostorF.glsl and the
alpha-impostor programs; shadow/velocity/depth shaders; look-only glow shaders; in actorghostF.glsl anything but the
§7.5 block + two uniforms + one helper (`ghostGlowOnly` path untouched); `registerFace` / `resolve_actor_fx_owners`;
in llvoavatar.cpp anything but the one `computeUpdatePeriod` line (`isImpostor`, `isVisuallyMuted` unchanged);
G-buffer formats and flag encoding (llshadermgr.cpp:687-718, pipeline.cpp:614-623); `LLGLSLShader::hash`; ReShade
bridge/add-on; Env Light Intensity v2 uniforms; the `enve` tree. In llactormover.cpp only §5.5/§5.6 uploads and
the `GhostDrawParams` field. No global GL state changes anywhere. (§7.5 block = the Brightness/Glow block after
:1471 plus the one-line debug override before :1539.)

## 15. Implementation order checklist (work top to bottom; anchors at HEAD `1ecdbbdc35c`)
Precedence: where an R-table and a section differ, the SECTION text (§1–§14) is authoritative; R-tables are history.
1. [ ] Model `alavatarlightresponsemodel.{h,cpp}` (§4 item 1, math §1.2) + `tests/alavatarlightresponsemodel_test.cpp` (§12.1); CMake lists (§4).
2. [ ] Store `alavatarlightresponse.{h,cpp}`: explicit vs inherit entries, table + `lookup`/`peek`, `keepsLive`, counters, `sEverActive`, load/save, `exportFor`/`importMerge`, impostor dirtying (§3, §4, §4.1).
3. [ ] Settings in app_settings/settings.xml (§8.3); load call at llstartup.cpp:1203.
4. [ ] GLSL module `avatarLightResponseF.glsl` (§6); register (llviewershadermgr.cpp:1328); attach (llshadermgr.cpp:218-227).
5. [ ] deferredUtil.glsl Keep refactors (§7.4 first bullet); old names forward with 1.0.
6. [ ] Deferred writers §7.1, including the debug-emission REPLACE line as the last statement of every `HAS_EMISSIVE` block, and the PBR carrier args.
7. [ ] Forward paths §7.2 (Keep calls, prototypes, final debug overrides).
8. [ ] Actor FX specifics §7.3: `actorFxGraphicCoverStrength()` in actorFxF.glsl + fallback; sharedActorFxPbrF graphic-Cover line; glow trims in sharedActorFxPbrGlowF, emissiveF/Indexed, pbrglowF/Indexed.
9. [ ] Deferred light readers §7.4 (softenLightF, multiPointLightF, pointLightF, spotLightF ×2).
10. [ ] actorghostF §7.5 (uniforms, helper, Brightness/Glow block after :1471, debug override before :1539).
11. [ ] CPU hooks: lldrawpool.{h,cpp} API, `upload_alr`, wrappers (§5.1-§5.3); lldrawpoolavatar.cpp (§5.4); llactormover.cpp shared replay (§5.5); pipeline.cpp proxies and `drawGeometryGhost` uploads to the BOUND program (`batch_shader` / `static_shader`, §5.6); `GhostDrawParams::mLightResponseKey` and its setters.
12. [ ] llvoavatar.cpp `computeUpdatePeriod` line (§2.3).
13. [ ] UI: floater + XML (§8.1); context menus + alviewermenu handler, menu_viewer item, Director row menu (§8.2); Director scene I/O (§8.4); notification `AvatarLightResponseResetAll`.
14. [ ] Instrument / log (§10).
15. [ ] Self-check against §11 and §14, then adversarial review; no build until review returns 0 must-fix.
