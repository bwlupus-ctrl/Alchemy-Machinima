# Clone overlay realism gate

Run from the worktree root, without a viewer build:

```powershell
python scripts/code_tools/clone_looks_gl_check.py
```

Both the normal and cinematic-only GL entry points run the permanent real SL
safety suite. The standalone command also regenerates the production runtime
default/clock evidence first:

```powershell
python scripts/code_tools/clone_looks_safety_check.py
```

`--looks` is a standalone development subset, never used by the normal gate.
The gate discovers every overlay ID >=64 in the Studio style selector; 68/72
are retired. Adding another selector entry automatically expands the matrix.
Only the eight dated authored-hair-alpha (look ID, check) pairs in `LEGACY_KNOWN` are reported as
`WARN`, with context in the JSON `legacy_summary`. The SHA-256 pin covers
the exact pairs, dates and reasons; any table edit requires explicit review.
A listed pair that starts passing fails as stale and must be removed. All
other failures, including every look >=85, remain fatal. Integrated negative
controls reject table growth, new checks, stale entries and failures on 85-100.
Fix a new look instead of weakening the limits. Every new anatomical motif
must extend the XML-derived semantic oracle and include a wrong-frame negative
control. Prim-local rigid faces must suppress anatomy or receive a body-space
reference before using it.

The reviewed animations for 64/66/67/71/76 additionally require perceptible
motion on the real SL avatar at 600 projected pixels. The metric pools absolute
display R/G/B channel errors inside the fixed neutral silhouette, composited on
RGB .035 and clipped to [0,1]. Every 2 s interval at 0/2/4/6/8/10 s, both seeds,
three poses/views and Effect fps 0/12 must have MAE >= .012 and p95 >= .04.
Glass is checked both with and without scene refraction. Two simultaneous
seeds at 2/4/6/8/10 s require MAE >= .01, also after subtracting each seed's
own t=0 still to isolate motion from existing seeded material patterns.
At fps 0, both MAE and p95 for adjacent 1/60 s frames must be strictly below
one quarter of the corresponding 2 s change. Effect fps 12 holds stay exact.
The same metrics for all IDs 64-100 (including retired 68/72) are report-only
outside these five IDs, in `perceptibility_table.md` and JSON. Existing safety
criteria for every other look remain unchanged. The five reviewed looks' t=0
real-SL-avatar stills are compared with pre-animation commit `69cfd4637fd`: display RGB
mean absolute error must be <= .01 and its 99th percentile <= .03 (display
RGB units, where 1.0 is reference white). Alpha stays exact at t=0, 2.37 and 9.97.
Every other look retains exact parity, including 85-100 against that base.
Before/after strips at 0/2/4/6 s and 2x2 simultaneous crowd seed tiles at 4 s
preserve the 600 px body height in cropped cells in `renders/legacy_anim/`,
alongside metrics.

## Fixtures and limits

The loader reads the shipped head, upper/lower body, both eyes, eyelashes,
hair and skirt from `indra/newview/character/avatar_*.llm`. Layout follows
`LLPolyMeshSharedData::loadMesh`, including the two byte flags, ignored XYZ
rotation-order byte, optional detail UVs, float weights, 16-bit triangle indices,
and 64-byte skin joint names. Morph blocks are not applied: these are the
unmorphed shipped bind meshes, not an arbitrary resident's baked shape.

The XML skeleton supplies joints, bind transforms, and +X forward / Z up.
Legacy adjacent-pair weights use the depth-first base-skeleton palette order
from `LLAvatarJointMesh::setupJoint`. They are converted to production
`objectSkinV` weight4 inputs. Mesh and skeleton retain the production ground
origin, without translation or rescaling. Only the camera is centred, using
each pose's skinned mesh bounds. This replays the system mesh as a worn rigged
clone through the actual `actorghostV/F` programs. The old
weighted LLM mesh-node Y=9 export offset is not a skinned vertex translation.
Both arms raised and a torso/head turn deform through the real joint palette.
Synthetic MASK/BLEND cards, a zero-alpha prim, and a non-unit rigid attachment
remain separate batches. Hidden body batches have poisoned RGB and zero
authored alpha; their submission must match collector omission exactly.

This is a driver-rendered shader safety gate, not an end-to-end viewer collector,
animation, bake, atmosphere, imported-avatar, or in-world capture test. Existing
viewer-path/runtime checks remain necessary. Production parameter defaults
are exported by the native runtime fixtures (including dissolve .5 and scrub
false); the material draw uses production style params (0,1,0,8), creative FX
off, brightness 1, and fully opaque operator strength to stress the treatment.
Time uses the real clock's fps floor law, checked separately by native fixtures.

Every look gets seven times across ten seconds, Effect fps 0/12, three poses,
front/profile/back, and projected body heights of 1800/600/130 pixels.
The 4K test is a central 2304x2160 crop of a 3840x2160 screen, with the exact
whole-screen coordinates and a 768-pixel X origin. Geometry has hundreds of
pixels of margin; neutral silhouettes must not touch the crop boundary.
No tested 4K pixels are downsampled. AA compares a half-pixel render to 16
subpixel samples at actual 1800-pixel body height, on all three camera views.

Fixed limits: any visible energy or coverage beyond a two-pixel dilation of the
neutral authored silhouette; any NaN/Inf; mean display luminance >.75;
more than 25% of silhouette pixels with every RGB channel >=.98; identical
time frames; identical clone seed frames; nonzero hidden/zero-alpha replay;
changed authored MASK/BLEND alpha; AA RMS >=.04 or p99 >=.15.
The luminance ceiling is calibrated to the neutral .42/.72/.94 identity tint
(Rec.709 Y=.672) plus .078 highlight headroom. It is fixed independently of
the measured looks. AA uses the existing cinematic envelope without changes.

In-memory negative shader variants must be rejected for displaced white
shells, static Magma, progress-parked Decode at real defaults, ignored texture
alpha, and -Y forward anatomy. Additional seed and nonfinite controls check
those classifiers. The anatomy oracle derives feet/ankles, knees, pelvis,
chest, shoulders, neck, head and eyes from XML joint chains. Head extent comes
from the HEAD collision volume. `clone_looks_anatomy.py --write` generates the
named GLSL constants; safety execution verifies the entire block against XML.
The production eye line is 1.762 m, pelvis 1.067 m and feet .006 m.

On real visible triangles the oracle measures Cold Gaze/Torn Husk eye peaks,
Emitter's foot pool and height ramp, Rapture's feet-to-head sweep, Aureole's
head/shoulder crown, Hunter's YZ coordinates and visible body span, and
Reform's seed-dependent foot-to-eye impact range. Old
fixture heights and wrong depth/forward axes must fail the same assertions.
The non-unit rigid attachment uses the actual non-skinned shader permutation:
Rapture planes, eye motifs, Torn Husk spine and Hunter brackets/ticks must
contribute exactly zero there. Other material terms retain the shared clock.
A temporary mod-4096 variant must fail the Rapture continuity control.

PNG strips, individual view/time PNGs, the contact sheet and JSON evidence go
to `.tmp/looks16/renders/safety/`. Results append incrementally to
`.tmp/looks16/REPORT_SAFETY.md`. Known legacy pairs are reported loudly as warnings; every other failed old
look and every failed new look must be fixed before the gate passes.

## Prebuild hook

The installed `C:/Users/xianw/cxs/prebuild_guard.ps1` owns the external `$kitPy`
list and is outside this worktree's writable scope. The checked-in list addition
is `scripts/code_tools/clone_looks_prebuild_gates.ps1`. Integrate it immediately
after the `$kitPy = @(...)` assignment and before `foreach ($g in $kitPy)`:

```powershell
. (Join-Path $repo 'scripts\code_tools\clone_looks_prebuild_gates.ps1')
```

This adds the full GL/runtime/safety command to the existing failure-collecting
prebuild list. Do not use `--looks` or skip parity in that list. The installed
guard must set `$repo` to the intended worktree. The external guard was only
read during implementation; no external files or build steps were modified.
