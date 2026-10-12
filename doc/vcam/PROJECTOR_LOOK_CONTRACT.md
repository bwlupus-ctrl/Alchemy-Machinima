# Projector look: the shared contract (engine job + UI job)

User request (10/12): make the VCam projector "more projector-like", with a Projector card exposing every setting, new creative effects and presets. No undo. Viewer-local. Empty states hide.

## Existing settings (llprismlens.h ProjectorSettings, already persisted)
mFitMode (FIT/FILL/STRETCH), mAnchor[2], mFeedEV (-6..+6), mLampPowerEV (-2..+8, default 0), mFollowMainExposure, mResolution (256/512/1024), mNoSignal (ENoSignal), mKeepGobo, mEffects (ScreenEffects: flips/rotation + all TV effects, same as display faces). Lamp power is a viewer-only `2^EV` multiplier on the bound projector light's surface and volumetric contribution; it never changes or updates the object.
If ScreenEffects does NOT contain flip H / flip V / rotate (0/90/180/270), add them to ProjectorSettings as mFlipH, mFlipV, mRotate (U8 quarter turns).

## NEW: struct ProjectorLook (member ProjectorSettings::mLook), persisted in the scene LLSD under "look"; missing key => defaults (old scenes identical)
| field | type / range | default | meaning |
|---|---|---|---|
| mKeystoneH | F32 -1..1 | 0 | horizontal keystone (trapezoid) |
| mKeystoneV | F32 -1..1 | 0 | vertical keystone |
| mShutterFlicker | F32 0..1 | 0 | film shutter strobe strength |
| mShutterHz | F32 12..48 | 24 | shutter rate |
| mGateWeave | F32 0..1 | 0 | frame wobble amount |
| mHotspot | F32 0..1 | 0 | centre-bright / edge falloff of the lamp |
| mLampKelvin | F32 2700..10000 | 6500 | lamp colour temperature (6500 = neutral) |
| mGelTint | LLColor3 | (1,1,1) | gel colour multiply |
| mDust | F32 0..1 | 0 | dust specks |
| mScratches | F32 0..1 | 0 | vertical film scratches |
| mBurn | F32 0..1 | 0 | burn-in / edge burn |
| mGrain | F32 0..1 | 0 | film grain |
| mHalation | F32 0..1 | 0 | glow bleeding around highlights |
| mThrowMode | enum EProjectorThrow { OFF, LIGHT_FOV, CAMERA_FOV } | OFF | lock the projected size to the light's FOV or the camera's |

API: LLPrismLens/ALVCamPageModel already have setProjectorSettings(handle, settings); the UI job uses that with the new fields. Defaults must render IDENTICALLY to today (all effects off => byte-identical feed).

## Presets (UI job: data in alvcampagemodel or a small table; names exactly)
- "Clean digital": everything default.
- "Cinema xenon": mLampKelvin 6000, mHotspot 0.25, mShutterFlicker 0.05, mHalation 0.15, mGrain 0.05.
- "16mm home movie": mLampKelvin 3400, mHotspot 0.45, mShutterFlicker 0.35, mShutterHz 18, mGateWeave 0.35, mDust 0.3, mScratches 0.2, mGrain 0.35, mHalation 0.25.
- "Old slide projector": mLampKelvin 3000, mHotspot 0.6, mKeystoneV 0.15, mDust 0.15, mBurn 0.2, mHalation 0.2.
- "Worn film reel": mLampKelvin 3800, mShutterFlicker 0.25, mGateWeave 0.25, mDust 0.5, mScratches 0.5, mBurn 0.35, mGrain 0.45.
Choosing a preset writes the look fields (it doesn't touch fit/anchor/flip/resolution).
