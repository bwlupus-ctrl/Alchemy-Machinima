#include "linden_common.h"
#include "../test/lltut.h"
#include "../alkitqol.h"

namespace tut
{
struct alkitqol_data {};
typedef test_group<alkitqol_data> alkitqol_group;
typedef alkitqol_group::object alkitqol_object;
alkitqol_group gAlKitQol("alkitqol");

template<> template<> void alkitqol_object::test<1>()
{
    ensure("shared precision accepts a value hidden at two decimals",
           ALKitQol::valuesEqual(LLSD(1.004), LLSD(1.0), 2));
    ensure("shared precision rejects the same value at three decimals",
           !ALKitQol::valuesEqual(LLSD(1.004), LLSD(1.0), 3));
}

template<> template<> void alkitqol_object::test<2>()
{
    const std::vector<ALKitQol::Favorite> input = {
        { "camera_group/vcam_tab", "vc_exposure" },
        { "light_group/cine_light_rig_tab", "cine_key_ev" }
    };
    ensure("favorites survive LLSD persistence", ALKitQol::favoritesFromLLSD(
        ALKitQol::favoritesToLLSD(input)) == input);
}

template<> template<> void alkitqol_object::test<3>()
{
    ensure("same clipboard types paste", ALKitQol::pasteTypesCompatible(LLSD(3), LLSD(1)));
    ensure("numeric clipboard types can be coerced", ALKitQol::pasteTypesCompatible(LLSD(3.6), LLSD(1)));
    ensure_equals("real paste rounds to integer", ALKitQol::coercePasteValue(LLSD(3.6), LLSD(1)).asInteger(), 4);
    ensure("non-numeric clipboard types do not paste", !ALKitQol::pasteTypesCompatible(LLSD("3"), LLSD(1.0)));
    ensure("undefined clipboard does not paste", !ALKitQol::pasteTypesCompatible(LLSD(), LLSD(false)));
}

template<> template<> void alkitqol_object::test<4>()
{
    const ALKitQol::Favorite proxy{ "quick_group/quick_tab", "quick_proxy_0_control" };
    const ALKitQol::Favorite source{ "camera_group/vcam_tab", "vc_exposure" };
    ensure("proxy favorite key is its source key",
           ALKitQol::sourceFavorite(proxy, source) == source);
}

template<> template<> void alkitqol_object::test<5>()
{
    LLSD stored = LLSD::emptyArray();
    stored.append(LLSD().with("page", "quick_group/quick_tab")
                        .with("widget", "quick_proxy_2_control"));
    stored.append(LLSD().with("page", "camera_group/vcam_tab")
                        .with("widget", "vc_exposure"));
    const std::vector<ALKitQol::Favorite> cleaned = ALKitQol::favoritesFromLLSD(stored);
    ensure_equals("junk Quick favorites are removed", cleaned.size(), size_t(1));
    ensure_equals("valid favorite survives cleanup", cleaned[0].widget, std::string("vc_exposure"));
}

template<> template<> void alkitqol_object::test<6>()
{
    LLSD stored = LLSD::emptyArray();
    auto add = [&stored](const char* page, const char* widget)
    {
        stored.append(LLSD().with("page", page).with("widget", widget));
    };
    add("world_group/atmosphere_tab", "ps_BDMergeProjectorVolumetricsMultiplier_slider");
    add("world_group/atmosphere_tab", "projector_cones_enable_check");
    add("world_group/atmosphere_tab", "soft_shadows_enable_check");
    add("world_group/atmosphere_tab", "soft_shadow_sun_check");
    add("world_group/atmosphere_tab", "gobo_anisotropic_check");
    add("world_group/atmosphere_tab", "projector_volumetrics_half_res_check");
    add("world_group/atmosphere_tab", "ps_BDMergeFroxelWindAzimuth_slider");
    // Atmosphere keeps its own rows.
    add("world_group/atmosphere_tab", "ch_CineHazeStrength_slider");
    add("world_group/atmosphere_tab", "localfog_master");
    // Another page with a lookalike widget name is not rewritten.
    add("light_group/practicals_tab", "ps_BDMergeProjectorVolumetricsMultiplier_slider");
    // After migration a stored duplicate of the same row collapses.
    add("light_group/projector_shafts_tab", "ps_BDMergeProjectorVolumetricsMultiplier_slider");

    const std::vector<ALKitQol::Favorite> cleaned = ALKitQol::favoritesFromLLSD(stored);
    const std::string shafts = "light_group/projector_shafts_tab";
    const std::vector<ALKitQol::Favorite> expected = {
        { shafts, "ps_BDMergeProjectorVolumetricsMultiplier_slider" },
        { shafts, "projector_cones_enable_check" },
        { shafts, "soft_shadows_enable_check" },
        { shafts, "soft_shadow_sun_check" },
        { shafts, "gobo_anisotropic_check" },
        { shafts, "projector_volumetrics_half_res_check" },
        { shafts, "ps_BDMergeFroxelWindAzimuth_slider" },
        { "world_group/atmosphere_tab", "ch_CineHazeStrength_slider" },
        { "world_group/atmosphere_tab", "localfog_master" },
        { "light_group/practicals_tab", "ps_BDMergeProjectorVolumetricsMultiplier_slider" },
    };
    ensure("moved favorites follow their rows, others stay, duplicates collapse", cleaned == expected);
}
} // namespace tut
