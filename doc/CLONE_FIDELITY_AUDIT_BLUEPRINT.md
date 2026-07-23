# Clone Fidelity Audit — Blueprint (Codex, 2026-07-23)

Read-only diagnostic: is the clone's harvested/resolved data identical to the source avatar's, mesh to
materials? DECIDES "real data divergence (fixable collection/resolution bug)" vs "data identical, only
the render differs (approximation / live-mutation -> locked clone)" for the WHOLE avatar. Codex session
019f8cc2. Implement -> MANDATORY Codex review -> 0 must-fix.

## Core: TWO-TIMEPOINT, read-only
1. EARLY capture during collectGhostBatches (the early harvest, llviewerdisplay ~943): snapshot VALUES
   (not raw ptr) into BatchSnapshot/MaterialSnapshot when the audit is armed.
2. LATE walk immediately BEFORE render_ui() (the OLD collector location, llviewerdisplay ~1124, while
   LLSpatialGroup::sNoDelete still true, current draw maps still own LLPointer<LLDrawInfo>): re-walk the
   source's live attachment draw maps, compare against harvested membership + early snapshots.
Directly tests the 3966415d7df timing hypothesis WITHOUT dereferencing stale pointers, AND compares the
ghost's effective binding vs the stock pool's effective binding. Command ARMS for the next full frame;
the walk happens at the late point (chat input is NOT a safe lifetime boundary).

## Files
NEW indra/newview/llclonefidelityaudit.{h,cpp} (LLCloneFidelityAudit singleton). Hooks: llactormover.cpp
collectGhostBatches (early capture) + extract the attachment walk; llactormover.h; llviewerdisplay.cpp
~1124 (runLateAuditIfPending); alchatcommand.cpp (/clonefidelity); CMakeLists.txt; settings.xml (opt cmd name).

## Shared walk (no duplicate logic)
Extract collectGhostBatches's attachment enumeration into ONE read-only visitor used by BOTH collector
and audit: walkGhostSourceGeometry(avatar, rigged_cb, static_cb). Must reproduce EXACTLY: av->
mAttachmentPoints, skip HUD APs, attachment root + children, skip dead objs/drawables, dedup spatial
groups, the exact kRiggedPasses (MOVE to a single shared definition), non-rigged face eligibility.
Rigged cb gets the owning LLPointer<LLDrawInfo>* + group + pass; static cb gets the LLFace*.

## Safe staleness (CRITICAL)
Late walk builds unordered_map<const LLDrawInfo*, LiveBatchRecord> live_by_address (each retaining an
LLPointer<LLDrawInfo> to PIN it during the report) + a structural multimap. For a harvested RAW ptr:
compare its numeric ADDRESS against live_by_address; NEVER dereference unless present; if absent ->
CLONE_ONLY_STALE using the EARLY value snapshot. Pointer compare is safe; deref of an absent address is
not. ABA caveat: even on address match, compare the early structural snapshot (VB/range/pass); mismatch
-> ADDRESS_REUSED_OR_MUTATED. Exact same-address same-fields ABA is unprovable without a generation id
on LLDrawInfo (document).

## Match hierarchy
Primary: same LLDrawInfo address + same pass -> IDENTITY_MATCH. Fallback structural key (wearer UUID +
root/group + pass + VB addr + start/end/count/offset) -> REBUILT_EQUIVALENT (multimap; identical
geometry can repeat). Verdicts: IDENTITY_MATCH, REBUILT_EQUIVALENT, CLONE_ONLY_STALE, SOURCE_ONLY_DROPPED,
PASS_RECLASSIFIED (same ptr diff pass), AMBIGUOUS_MATCH.

## Collector dedup audit
The collector dedups by VB+range WITHOUT pass. Model both raw source truth (every entry every pass) AND
collector result. Two source records sharing geometry in DIFFERENT passes -> DROP_DEDUP
conflicting_passes=[...] (esp. base+glow) -- NOT a normal missing batch.

## Decisive binding comparison (the crux) -- section 8
Two PURE non-GL resolvers (NEVER call bind()/bindBumpMap()/shader/tex-unit methods -- only COMPUTE what
they'd select): resolveGhostBinding(snapshot, sweep, slot) vs resolveStockBinding(live, pass, slot).
Return ResolvedSurfaceBinding {color/normal/spec-or-ORM/emissive TextureRef + factors + cutoff +
4 transforms + double-sided + reason}. TextureRef distinguishes: real UUID/ptr / default-white /
default-flat-normal / null-gap / media-override / "leave unit unchanged" (stock indexed legacy).
- Scalar GLTF: stock base = di->mTexture(media) else material base else white; emissive = di->mTexture
  else material emissive else white. GHOST base sweep matches stock base; GHOST glow sweep uses material
  emissive NOT di->mTexture -> under a media override, report BIND_DIFF map=emissive.
- Indexed GLTF: per slot base/normal/ORM/emissive material-or-default; NO scalar media override. Normalize
  null->the white it binds; log raw null vs fallback.
- Scalar legacy/simple/fullbright/alpha/bump: effective color = di->mTexture; compare tex matrix + cutoff.
  Bump: report source diffuse + mBump + source tex used to derive bump; do NOT generate/bind bump image.
- Indexed legacy: stock diffuse per slot = mMaterialSlotList[s].mDiffuse else white vs ghost_batch_slot_
  texture(); also compare normal/spec inputs (equality distinguishes "data identical, overlay unlit" from
  missing data). Generic mTextureList: per vertex-selected slot; null stock slots may be inherited ->
  SOURCE_UNDEFINED_INHERITED not white.
- Static: ghost color = fetched GLTF base else face->getTexture() else white; STOCK color must come from
  the face's LIVE mDrawInfo + getTextureIndex() (scalar/indexed stock resolver) -- this is where a static
  media override / indexed face decisively disagrees with the ghost static path.

## Field snapshot (early + late)
Per batch: LLDrawInfo addr, group addr, root UUID/name, object UUID/name, pass name+val, VB addr,
start/end/count/offset, VB type mask, TYPE_TEXTURE_INDEX present, mTextureMatrix present+VALUE HASH,
mModelMatrix present+hash, mAvatar UUID, skin hash, mMaterialID, mShaderMask, mFullbright, mHasGlow,
mBump, mShiny, mDiffuseAlphaMode, mAlphaMaskCutoff, blend src/dst, normal/spec tex UUIDs. Use matrix
VALUE HASHES not just ptrs (may mutate in place). Texture logging: uuid + ptr + null/default class +
list type. Scalar GLTF: mMaterialID, ptr, mGLTFMaterial->getHash() (resolved-content id), fetch/loaded
state, alpha mode/cutoff, double-sided, base/emissive/metallic/roughness factors, base/normal/MR/emissive
tex UUIDs, 4 KHR transforms, di->mTexture media-override UUID. Indexed GLTF: per slot incl null gaps +
over-sIndexedGLTFChannels flag. Indexed legacy: per mMaterialSlotList entry (diffuse/normal/spec UUIDs,
spec color/gloss, env intensity, cutoff, fullbright). Static: face addr, root UUID, face index, VB, geom
range, face->getTexture UUID, TE image UUID, TE color/alpha, getTextureIndex, mDrawInfo addr, pool type,
tex/render matrix hash, GLTF ptr+getHash + fetched base/normal/ORM/emissive, factors, alpha, ds, legacy
mat IDs+alpha, collector mAlphaKind/cutoff/ds.

## Diff verdicts (a batch can carry several)
MATCH_DATA_AND_BINDING, MATCH_DATA_BINDING_DIFF, MATCH_BINDING_APPROXIMATION_ONLY, EARLY_TO_LATE_MUTATION,
REBUILT_EQUIVALENT, SOURCE_ONLY_DROPPED, CLONE_ONLY_STALE, PASS_RECLASSIFIED, DROP_DEDUP,
STATIC_CLASSIFICATION_DIFF, AMBIGUOUS, UNVERIFIABLE. e.g. EARLY_TO_LATE_MUTATION | MATCH_LATE_BINDING |
DEFERRED_PHASE_RISK = late overlay sees current data but the earlier deferred submission consumed old.

## Command + selection
/clonefidelity | /clonefidelity all | /clonefidelity <instance-uuid>. Selection: explicit uuid ->
ALGhostStudio::getSelected() -> nearest enabled GHOST_STYLE_CLONE to camera -> "no enabled Clone
instance". Default ONE clone (all = huge log). Multiple instances sharing a source: ONE source audit,
reported per instance with metadata (uuid, style, LIVE/FROZEN [frozen still uses LIVE material ptrs --
only palettes+attach mats frozen -- call out], deferred on/off, placement, source uuid). Command returns
true immediately + posts "[Clone Fidelity] Audit armed for next complete render frame: <instance>."

## Output
CHAT concise: "[Clone Fidelity] Clone 8f2 source Alice, frame N / Attachments A rigged B static C /
Exact X binding-diffs Y mutated Z dropped D / stale S approx-only P / DIFF Headband: bind=2 dropped=1".
All-match: "DATA/BINDINGS PASS: N surfaces matched. This does not assert pixel-identical rendering."
LOG: one stable line per batch/slot (instance/source/attachment/root/object/kind/status/pass/early_di/
late_di/match/vb/range/slot/material_id/material_hash/source_color/ghost_color/source_xform/ghost_xform/
texture_index). Stale line uses early snapshot, late_membership=false, NEVER derefs the stale addr.

## Proves / does NOT prove (do not over-read)
PROVES: collector missed entries / retained gone entries / rebuilt-between-phases / material fields
changed / pass reclassified / dedup dropped a required pass / ghost vs stock pick different effective
textures / indexed slot lists incomplete-misaligned-overcap / alpha-cutoff-factors-transforms differ /
static ghost ignores a source override. CANNOT PROVE: pixels identical / lighting-tonemap-exposure-probe-
blend match / translucent order / overlay depth-prime vs world depth / fullbright reproduces PBR /
deferred shadows-probes / frozen material immutability (current frozen clones do NOT freeze materials).
INTERPRETATION: data/binding diff => concrete fixable collection/resolution bug. data+binding equal but
factors/transforms differ => fixable ghost draw-path approximation. all inputs equal, pixels differ =>
render-path/state/order/lighting -> pixel diagnostics next. historical material immutability => locked
clone. DO NOT auto-conclude "locked clone" just because UUIDs match -- next suspects are UV transforms,
color factors, alpha, draw order, shader behavior.
