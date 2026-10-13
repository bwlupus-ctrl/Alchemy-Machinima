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
    ensure("float tolerance accepts display-equivalent values",
           ALKitQol::valuesEqual(LLSD(1.0004), LLSD(1.0), 3));
    ensure("float tolerance rejects visible changes",
           !ALKitQol::valuesEqual(LLSD(1.002), LLSD(1.0), 3));
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
    ensure("different clipboard types do not paste", !ALKitQol::pasteTypesCompatible(LLSD(3), LLSD(1.0)));
    ensure("undefined clipboard does not paste", !ALKitQol::pasteTypesCompatible(LLSD(), LLSD(false)));
}
} // namespace tut
