# Integrating the SL bridge into a local Launchpad (advanced / personal use)

> ⚠️ **License:** `MartysMods_LAUNCHPAD.fx` is proprietary (© Pascal Gilcher,
> "all rights reserved, unauthorized copying prohibited"). A modified Launchpad
> **cannot be distributed** and must be re-applied on every iMMERSE update. The
> shippable path is the standalone `SL_GBufferProvider.fx`. This guide is only
> for a personal build that fully *replaces* Launchpad's optical flow to reclaim
> its memory. All code blocks below are our own; they only reference (never
> reproduce) Marty's passes by anchor.

This folds normals + motion + **albedo** into Launchpad behind `SL_ENABLE_BRIDGE`,
fixes the stale-motion bug, and adds a debug technique that actually displays.

---

## 0. The stale-motion fix (REQUIRED)

Because the bridge build removes Launchpad's optical-flow passes, `PS_ProvideMotion`
becomes the *sole writer* of `Deferred::MotionVectorsTex`, which is never cleared.
Discarding on zero-delta therefore leaves **last frame's motion frozen** on static
pixels → RTGI smears when the camera stops. Fix: always write (0 is correct now).

Replace `PS_ProvideMotion` with:

```hlsl
void PS_ProvideMotion(in VSOUT i, out float2 o : SV_Target)
{
    // Sole writer of MotionVectorsTex in bridge mode -> must always write; a
    // discard here would freeze stale motion on static pixels (ghosting).
    float2 d = tex2Dlod(SL_sMotionPoint, float4(SL_UV(i.uv), 0, 0)).xy;
    o = sl_to_immerse_motion(d);
}
```

> Note the asymmetry: the **normals** override keeps its `discard` on purpose —
> Launchpad's normal passes still run, so discard = graceful fallback to them.
> Only motion lost its underlying writer.

---

## 1. Albedo → colored GI (NEW)

Overlay approach (mirrors normals): Launchpad's albedo passes still run, we
override on top, and `discard` on black keeps Launchpad's albedo for sky and
gives a full no-op fallback when the bridge is dead.

### 1a. Preprocessor (next to `SL_ENABLE_BRIDGE`)

```hlsl
// Provide real unlit albedo into Deferred::AlbedoTex for true colored GI bounce.
#ifndef SL_PROVIDE_ALBEDO
 #define SL_PROVIDE_ALBEDO 1
#endif
// iMMERSE mixes albedo with LINEAR GI. If colored bounce looks too dark/saturated
// the SL gbuffer albedo is sRGB-encoded -> set this to 1.
#ifndef SL_ALBEDO_TO_LINEAR
 #define SL_ALBEDO_TO_LINEAR 0
#endif
```

### 1b. Sampler + pixel shader (in the `SL Bridge Functions` `#if SL_ENABLE_BRIDGE` block)

```hlsl
#if SL_PROVIDE_ALBEDO
sampler SL_sAlbedoPoint
{
    Texture = SLAlbedoTex;
    MinFilter = POINT; MagFilter = POINT; MipFilter = POINT;
    AddressU = CLAMP; AddressV = CLAMP;
};
void PS_ProvideAlbedo(in VSOUT i, out float3 o : SV_Target)
{
    float3 a = tex2Dlod(SL_sAlbedoPoint, float4(SL_UV(i.uv), 0, 0)).rgb;
    if (a.r == 0.0 && a.g == 0.0 && a.b == 0.0) discard;  // sky/unbound -> keep Launchpad albedo
#if SL_ALBEDO_TO_LINEAR
    a = pow(a, 2.2);
#endif
    o = a;
}
#endif
```

### 1c. Technique — add ONE pass immediately AFTER Marty's

`pass { ... PixelShader = AlbedoMainPS; RenderTarget = Deferred::AlbedoTex; }`

```hlsl
#if SL_PROVIDE_ALBEDO
    pass {VertexShader = MainVS; PixelShader = PS_ProvideAlbedo; RenderTarget = Deferred::AlbedoTex;}
#endif
```

---

## 2. Working debug/test view (NEW)

The in-Launchpad `PS_ShowReceived` pass can never display: Launchpad is the
top-of-list prepass, so anything it writes to the backbuffer is painted over by
RTGI et al. Move it into its **own technique** that you enable and drag to the
BOTTOM of the list.

### 2a. Extend the combo (add the albedo entry)

```hlsl
    ui_items = "Off\0Normals iMMERSE receives\0Motion iMMERSE receives\0Albedo iMMERSE receives\0";
```

### 2b. Extend `PS_ShowReceived` (add the albedo branch before the `else`)

```hlsl
    else if (SL_DEBUG_VIEW == 3)     // albedo iMMERSE receives (unlit surface color)
    {
        o = Deferred::get_albedo(i.uv);
    }
```

### 2c. REMOVE the debug pass from the Launchpad technique

Delete this line from inside `technique MartysMods_Launchpad`:

```hlsl
    pass {VertexShader = MainVS;PixelShader = PS_ShowReceived; }
```

### 2d. ADD a standalone debug technique at the very END of the file

```hlsl
#if SL_ENABLE_BRIDGE
technique SL_Bridge_Debug
<
    ui_label = "SL Bridge: Debug View (move to BOTTOM of list)";
    ui_tooltip = "Shows what iMMERSE received from the SL bridge after all effects.\n"
                 "Enable, drag to the very bottom, pick a buffer in Launchpad's DEBUG view.";
>
{
    pass { VertexShader = MainVS; PixelShader = PS_ShowReceived; }
}
#endif
```

To test: enable **SL_Bridge_Debug**, drag it to the bottom, then use Launchpad's
**DEBUG view** dropdown. Normals should match `SL_BridgeDebug` (R/G equal, blue
inverted); Motion hue steady while panning; Albedo = flat unlit surface color.
Set DEBUG view → Off (or disable the technique) for normal rendering.

---

## 3. Optional: reclaim the albedo pyramid too (max efficiency)

Section 1 leaves Launchpad's ~20 albedo passes running (we overwrite their
result). To skip them like the optical flow, wrap Marty's albedo pass block in
`#if !SL_PROVIDE_ALBEDO ... #endif`. Trade-off: you lose the sky/unbound
fallback, so the `PS_ProvideAlbedo` `discard` should then be removed (write the
value). Only worth it if you always run with `BDMergeVelocityBuffer` on and the
bridge feeding. Left as opt-in because the overlay form is safer.

## 4. Notes carried over

- With OF removed, there is **no motion fallback** — `BDMergeVelocityBuffer`
  must be ON in the viewer or you get zero motion.
- `WriteCurrDepthPS`/`LinearDepthCurr` appears vestigial on the bridge path
  (its only readers were the OF filters). Safe to leave; remove only after
  confirming nothing downstream samples `sLinearDepthCurr`.
