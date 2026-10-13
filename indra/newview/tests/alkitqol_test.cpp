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
} // namespace tut
