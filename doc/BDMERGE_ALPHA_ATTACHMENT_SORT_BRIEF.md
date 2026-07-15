# BDMerge — Alpha Attachment-Sort (gated 3-pass ordering)

**Status:** implemented, gated default-off. Setting: `BDMergeAlphaAttachmentSort` (Boolean, default 0).
**Origin:** port of the AYAstorm double-alpha fix (mayatonton/phoenix-firestorm PR #122),
adapted to our stock alpha baseline.

## Problem it addresses
Stock above-water alpha renders **rigged first (writes depth), then all non-rigged**. Rigged
hair writing depth can z-reject non-rigged alpha behind it, and worn **non-rigged attachment**
alpha prims (eyelash prims, brow prims, lace, etc.) can be over-blended by rigged hair — the
classic "double alpha block" where attachment prims sit behind hair or hair edges show the
opaque scene through them.

Users have been reaching for **Force Mask** on hair as a workaround for this *sorting* problem.
This feature fixes the sorting directly, which should reduce the need to force-mask.

## Mechanism
When `BDMergeAlphaAttachmentSort` is ON, the **POST_WATER** (above-water), non-HUD forward alpha
pass is split into three sub-passes, discriminated by `LLDrawInfo::mAttachedToAvatar`:

1. **SIM non-rigged** (`ATTACHMENT_NONE`, `mAttachedToAvatar == false`) — background windows/foliage
2. **Rigged** (hair / clothing) — writes depth
3. **Worn attachment non-rigged** (`ATTACHMENT_ONLY`, `mAttachedToAvatar == true`) — eyelash prims etc.

So SIM background alpha still precedes hair, but worn attachment prims now draw **after** rigged
hair and land in front of it instead of being over-blended away.

Gate OFF == stock order (rigged, then all non-rigged) and is **byte-identical**. PRE_WATER keeps
the stock rigged-first order (needed for water-fog depth). HUD stays a single non-rigged pass.

## Code sites
- `llspatialpartition.h` — new `bool LLDrawInfo::mAttachedToAvatar` field.
- `llvovolume.cpp` `LLVolumeGeometryManager::registerFace` — set from
  `facep->getViewerObject()->getAvatar() != nullptr` (getAvatar walks parents, so non-rigged
  prims of a worn attachment resolve to the wearer; SIM objects resolve to null).
- `lldrawpoolalpha.h` — `enum AttachmentFilter { ATTACHMENT_ALL, ATTACHMENT_NONE, ATTACHMENT_ONLY }`;
  `forwardRender(bool rigged, AttachmentFilter)` and `renderAlpha(..., AttachmentFilter)`.
- `lldrawpoolalpha.cpp` — gated 3-pass dispatch in `renderPostDeferred`; filter plumbed through
  `forwardRender` → `renderAlpha`; per-batch skip in the non-rigged draw loop; debug-highlight
  fires only on the attachment pass (`ATTACHMENT_ALL || ATTACHMENT_ONLY`) so it isn't drawn twice.
- `settings.xml` — `BDMergeAlphaAttachmentSort`.

## Divergence from AYAstorm PR #122
- AYAstorm layered this on top of their earlier "§5 swap" (all-non-rigged-before-rigged). We
  never applied §5 swap, so our gate-ON path *is* the combined final ordering, and gate-OFF is
  clean stock. Their `mAttachedToAvatar` was a UUID; ours is a bool (only isNull/notNull was ever
  used). Their alpha-plate/transparent-DoF (`mAYAAlphaColor`) machinery is not present in our tree
  and is not required — this port only touches draw ordering.

## Test / shakedown (owed, in-world)
1. Toggle `BDMergeAlphaAttachmentSort` on. Worn hair + eyelash/brow prims: prims should sit in
   front of hair, not vanish behind it.
2. Compare against Force Mask on the same hair — see whether sorting alone is enough (goal: retire
   the force-mask workaround for hair).
3. Verify gate OFF is visually unchanged vs a prior build (byte-identical contract).
4. Watch for regressions on SIM alpha through hair (windows/foliage behind an avatar).
