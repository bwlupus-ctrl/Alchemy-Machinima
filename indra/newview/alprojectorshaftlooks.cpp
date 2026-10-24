/**
 * @file alprojectorshaftlooks.cpp
 * @brief Projector-shaft look data (pure: no llui, no gSavedSettings).
 *
 * A look owns LOOK settings, not performance work. Ownership is defined once, in ownedSettings().
 * The tables below are parsed by scripts/code_tools/relight_volumetric_gl_check.py: keep the row
 * format {"Setting", Type::X, {v}} one row per line, and one array per look.
 */

#include "linden_common.h"

#include "alprojectorshaftlooks.h"

#include <cstring>

namespace ALProjectorShaftLooks
{
namespace
{

// 1. Clean Beam
static const Value kLookClean[] = {
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {20.f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {12.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.1f}},
};

// 2. Cinematic Haze
static const Value kLookCinematic[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.15f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {24.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.74f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.14f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {1.25f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {1.2f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.25f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {20.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {3.5f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {7.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.05f}},
};

// 3. Stage Haze
static const Value kLookStage[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.25f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {24.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.82f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.18f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {2.2f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.25f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.6f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.2f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsFogGroundDensity", Type::F32, {1.5f}},
    {"BDMergeProjectorVolumetricsFogFalloff", Type::F32, {10.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {20.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {4.f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {8.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.06f}},
};

// 4. Clear Night Air
static const Value kLookClearNight[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {20.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.58f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.08f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {0.3f}},
};

// 5. Searchlight
static const Value kLookSearchlight[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.6f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {24.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.9f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.05f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {0.7f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.08f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {1.2f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.2f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {32.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {0.f}},
    {"BDMergeProjectorVolumetricsEquiangular", Type::BOOL, {1.f}},
};

// 6. Dust Storm
static const Value kLookDustStorm[] = {
    {"BDMergeProjectorVolumetricsMaxDistance", Type::F32, {14.f}},
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.1f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {36.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.68f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.28f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {3.f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.7f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.45f}},
    {"BDMergeProjectorVolumetricsDust", Type::BOOL, {1.f}},
    {"BDMergeProjectorVolumetricsDustIntensity", Type::F32, {1.f}},
    {"BDMergeProjectorVolumetricsDustScale", Type::F32, {0.6f}},
    {"BDMergeProjectorVolumetricsDustDrift", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.55f}},
    {"BDMergeProjectorVolumetricsFogGroundDensity", Type::F32, {3.f}},
    {"BDMergeProjectorVolumetricsFogFalloff", Type::F32, {6.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.18f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {16.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {4.5f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {9.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.06f}},
};

// 7. Dream Mist
static const Value kLookDreamMist[] = {
    {"BDMergeProjectorVolumetricsMaxDistance", Type::F32, {18.f}},
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {0.9f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {24.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.74f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.34f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {0.65f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.2f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.08f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.75f}},
    {"BDMergeProjectorVolumetricsFogGroundDensity", Type::F32, {2.2f}},
    {"BDMergeProjectorVolumetricsFogFalloff", Type::F32, {12.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.5f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {16.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {5.f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {9.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.08f}},
};

// 8. Silhouette Rays
static const Value kLookSilhouette[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.3f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {40.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.88f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.1f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {1.6f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.6f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {48.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {4.f}},
    {"BDMergeProjectorVolumetricsShadowTint", Type::F32, {0.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {0.f}},
    {"BDMergeProjectorVolumetricsRimStrength", Type::F32, {0.5f}},
    {"BDMergeProjectorVolumetricsRimPower", Type::F32, {3.f}},
    {"BDMergeProjectorVolumetricsRimWrap", Type::F32, {0.2f}},
};

// 9. Pipe Lights in Fog
static const Value kLookPipeFog[] = {
    {"BDMergeProjectorVolumetricsMaxDistance", Type::F32, {6.f}},
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.4f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {20.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.45f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.3f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {3.2f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.5f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.1f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsFogGroundDensity", Type::F32, {3.f}},
    {"BDMergeProjectorVolumetricsFogFalloff", Type::F32, {3.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {24.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {1.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {3.f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {6.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.15f}},
};

// 10. Smoky Club
static const Value kLookSmokyClub[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.3f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {20.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {2.6f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.55f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.45f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsFogStrength", Type::F32, {0.4f}},
    {"BDMergeProjectorVolumetricsFogGroundDensity", Type::F32, {2.f}},
    {"BDMergeProjectorVolumetricsFogFalloff", Type::F32, {4.f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.45f}},
    {"BDMergeProjectorVolumetricsBloomFeedAnamorphic", Type::F32, {1.6f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {24.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {1.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {0.f}},
};

// 11. Cathedral God Rays
static const Value kLookCathedral[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.2f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {32.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.86f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.06f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {0.9f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.18f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.9f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.04f}},
    {"BDMergeProjectorVolumetricsDust", Type::BOOL, {1.f}},
    {"BDMergeProjectorVolumetricsDustIntensity", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsDustScale", Type::F32, {0.5f}},
    {"BDMergeProjectorVolumetricsDustDrift", Type::F32, {0.03f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.3f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {24.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {3.f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {2.f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {5.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.04f}},
};

// 12. Underwater
static const Value kLookUnderwater[] = {
    {"BDMergeProjectorVolumetricsMaxDistance", Type::F32, {10.f}},
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.1f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {24.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.92f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.25f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {2.4f}},
    {"BDMergeProjectorVolumetricsTint", Type::COLOR3, {0.35f, 0.85f, 0.9f}},
    {"BDMergeProjectorVolumetricsTintStrength", Type::F32, {0.55f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.4f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {0.3f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.12f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.35f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {16.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeProjectorVolumetricsShadowTint", Type::F32, {0.15f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {5.f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {10.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.2f}},
};

// 13. Stained Glass
static const Value kLookStainedGlass[] = {
    {"BDMergeProjectorVolumetricsMultiplier", Type::F32, {1.2f}},
    {"BDMergeProjectorVolumetricsResolution", Type::U32, {32.f}},
    {"BDMergeProjectorVolumetricsAnisotropy", Type::F32, {0.8f}},
    {"BDMergeProjectorVolumetricsFeather", Type::F32, {0.05f}},
    {"BDMergeProjectorVolumetricsDensity", Type::F32, {1.1f}},
    {"BDMergeProjectorVolumetricsNoiseStrength", Type::F32, {0.08f}},
    {"BDMergeProjectorVolumetricsNoiseScale", Type::F32, {1.f}},
    {"BDMergeProjectorVolumetricsNoiseSpeed", Type::F32, {0.05f}},
    {"BDMergeProjectorVolumetricsBloomFeed", Type::F32, {0.2f}},
    {"BDMergeProjectorVolumetricsMaxLuminance", Type::F32, {20.f}},
    {"BDMergeProjectorVolumetricsShadowSamples", Type::U32, {2.f}},
    {"BDMergeProjectorVolumetricsShadowTint", Type::F32, {0.45f}},
    {"BDMergeSoftProjectorShadows", Type::BOOL, {1.f}},
    {"BDMergeSoftShadowSoftness", Type::F32, {1.5f}},
    {"BDMergeSoftShadowMaxPenumbra", Type::F32, {4.f}},
    {"BDMergeSoftShadowFill", Type::F32, {0.05f}},
};

// Looks: id, label, description, values.
static const Look kLooks[] = {
    {"b:clean", "Clean Beam", "Neutral shaft in light haze; the starting point for most scenes.", kLookClean, sizeof(kLookClean) / sizeof(kLookClean[0])},
    {"b:cinematic", "Cinematic Haze", "Soft filmic haze with gentle drifting dust and a subtle halo.", kLookCinematic, sizeof(kLookCinematic) / sizeof(kLookCinematic[0])},
    {"b:stage", "Stage Haze", "Theatre haze machine: even, dense air that shows every beam edge.", kLookStage, sizeof(kLookStage) / sizeof(kLookStage[0])},
    {"b:clear_night", "Clear Night Air", "Barely-there air: the cone reads, the set stays crisp.", kLookClearNight, sizeof(kLookClearNight) / sizeof(kLookClearNight[0])},
    {"b:searchlight", "Searchlight", "Long, tight, hard-edged beam reaching up into the night sky.", kLookSearchlight, sizeof(kLookSearchlight) / sizeof(kLookSearchlight[0])},
    {"b:dust_storm", "Dust Storm", "Dense, wind-driven dust with visible specks and a short, heavy beam.", kLookDustStorm, sizeof(kLookDustStorm) / sizeof(kLookDustStorm[0])},
    {"b:dream_mist", "Dream Mist", "Broad, low-contrast mist with a soft halo and slow drift.", kLookDreamMist, sizeof(kLookDreamMist) / sizeof(kLookDreamMist[0])},
    {"b:silhouette", "Silhouette Rays", "One hot backlight behind props: crisp shadow lanes fan out through thick, even haze.", kLookSilhouette, sizeof(kLookSilhouette) / sizeof(kLookSilhouette[0])},
    {"b:pipe_fog", "Pipe Lights in Fog", "Industrial tube and pipe lights glowing through thick, low fog; short heavy beams.", kLookPipeFog, sizeof(kLookPipeFog) / sizeof(kLookPipeFog[0])},
    {"b:smoky_club", "Smoky Club", "Thick rolling smoke for many moving beams, with a streaky stage glow.", kLookSmokyClub, sizeof(kLookSmokyClub) / sizeof(kLookSmokyClub[0])},
    {"b:cathedral", "Cathedral God Rays", "Long window shafts through still, dusty air with slow hanging specks.", kLookCathedral, sizeof(kLookCathedral) / sizeof(kLookCathedral[0])},
    {"b:underwater", "Underwater", "Teal, dense water with strong forward glow and slow drifting particles.", kLookUnderwater, sizeof(kLookUnderwater) / sizeof(kLookUnderwater[0])},
    {"b:stained_glass", "Stained Glass", "Coloured gobo shafts whose shadows carry the window's colour, steady in motion.", kLookStainedGlass, sizeof(kLookStainedGlass) / sizeof(kLookStainedGlass[0])},
};

// Every setting a look owns, once. Excludes the Dust permutation (see dustSetting()).
// Deliberately NOT owned: HalfRes, Temporal*, Scissor, Adaptive, MinResolution, Dither, TemporalDither,
// FrustumClip, ShadowJitterTap, FogBase, PyramidScissor, ConservativeShadow, BDMergeSoftShadowSun,
// BDMergeGoboAnisotropic (quality or calibration; a look never changes them), and the master enables.
static const char* const kOwned[] = {
    "BDMergeProjectorVolumetricsMultiplier",
    "BDMergeProjectorVolumetricsDensity",
    "BDMergeProjectorVolumetricsAnisotropy",
    "BDMergeProjectorVolumetricsTint",
    "BDMergeProjectorVolumetricsTintStrength",
    "BDMergeProjectorVolumetricsMaxLuminance",
    "BDMergeProjectorVolumetricsMaxDistance",
    "BDMergeProjectorVolumetricsFeather",
    "BDMergeProjectorVolumetricsBloomFeed",
    "BDMergeProjectorVolumetricsBloomFeedAnamorphic",
    "BDMergeProjectorVolumetricsRimStrength",
    "BDMergeProjectorVolumetricsRimPower",
    "BDMergeProjectorVolumetricsRimWrap",
    "BDMergeProjectorVolumetricsRimThreshold",
    "BDMergeProjectorVolumetricsRimSoftness",
    "BDMergeProjectorVolumetricsNoiseStrength",
    "BDMergeProjectorVolumetricsNoiseScale",
    "BDMergeProjectorVolumetricsNoiseSpeed",
    "BDMergeProjectorVolumetricsDustIntensity",
    "BDMergeProjectorVolumetricsDustScale",
    "BDMergeProjectorVolumetricsDustDrift",
    "BDMergeProjectorVolumetricsFogStrength",
    "BDMergeProjectorVolumetricsFogGroundDensity",
    "BDMergeProjectorVolumetricsFogFalloff",
    "BDMergeProjectorVolumetricsShadowSamples",
    "BDMergeProjectorVolumetricsShadowTint",
    "BDMergeProjectorVolumetricsResolution",
    "BDMergeProjectorVolumetricsEquiangular",
    "BDMergeProjectorVolumetricsAirborneTap",
    "BDMergeProjectorVolumetricsCookieFootprint",
    "BDMergeSoftProjectorShadows",
    "BDMergeSoftShadowSoftness",
    "BDMergeSoftShadowMaxPenumbra",
    "BDMergeSoftShadowFill",
};

struct LegacyId
{
    const char* old_id;
    const char* look_id;
};
// Old built-in ids -> looks. Fast Preview (0) was a quality preset; quality is no longer owned.
static const LegacyId kLegacy[] = {
    {"0", "b:clean"},       // Fast Preview
    {"1", "b:clean"},       // Balanced
    {"2", "b:cinematic"},   // Cinematic
    {"3", "b:stage"},       // Hazy Stage
    {"4", "b:clear_night"}, // Clear Air
    {"5", "b:searchlight"}, // Searchlight
    {"6", "b:dust_storm"},  // Dust Storm
    {"7", "b:dream_mist"},  // Dream Mist
};
} // namespace

const std::vector<Look>& looks()
{
    static const std::vector<Look> all(kLooks, kLooks + sizeof(kLooks) / sizeof(kLooks[0]));
    return all;
}

const Look* findLook(const std::string& id)
{
    for (const Look& look : looks())
    {
        if (id == look.id)
        {
            return &look;
        }
    }
    return nullptr;
}

const std::vector<std::string>& ownedSettings()
{
    static const std::vector<std::string> owned(kOwned, kOwned + sizeof(kOwned) / sizeof(kOwned[0]));
    return owned;
}

const char* dustSetting()
{
    return "BDMergeProjectorVolumetricsDust";
}

std::string canonicalLookId(const std::string& id)
{
    if (findLook(id))
    {
        return id;
    }
    for (const LegacyId& legacy : kLegacy)
    {
        if (id == legacy.old_id)
        {
            if (id == "0")
            {
                static bool logged = false;
                if (!logged)
                {
                    logged = true;
                    LL_INFOS("DirectorKit") << "Projector shaft preset Fast Preview was a quality preset; "
                                            << "quality is no longer part of a look, so it applies Clean Beam." << LL_ENDL;
                }
            }
            return legacy.look_id;
        }
    }
    return std::string();
}

}
