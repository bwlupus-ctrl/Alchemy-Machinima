#ifndef AL_KIT_QOL_H
#define AL_KIT_QOL_H

#include "llsd.h"
#include "llsdutil.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace ALKitQol
{
// Text rows read the system clipboard at click time. Other rows retain the
// typed value clipboard. Every route uses the owner's ordinary commit path.
template<class ReadText, class Commit>
bool pasteRowValue(bool textRow, const LLSD& valueClipboard, ReadText readText, Commit commit)
{
    const LLSD value = textRow ? readText() : valueClipboard;
    if (!value.isDefined() || (textRow && (!value.isString() || value.asString().empty()))) return false;
    return commit(value);
}

struct Favorite
{
    std::string page;
    std::string widget;

    bool operator==(const Favorite& other) const
    { return page == other.page && widget == other.widget; }
};

inline bool valuesEqual(const LLSD& left, const LLSD& right, S32 decimals)
{
    if (left.type() == LLSD::TypeReal || right.type() == LLSD::TypeReal)
    {
        const F64 scale = std::pow(10., decimals);
        return llround(left.asReal() * scale) == llround(right.asReal() * scale);
    }
    if (left.isArray() && right.isArray() && left.size() == right.size())
    {
        for (S32 index = 0; index < left.size(); ++index)
            if (!valuesEqual(left[index], right[index], decimals))
                return false;
        return true;
    }
    if (left.isMap() && right.isMap() && left.size() == right.size())
    {
        for (LLSD::map_const_iterator it = left.beginMap(); it != left.endMap(); ++it)
            if (!right.has(it->first) || !valuesEqual(it->second, right[it->first], decimals))
                return false;
        return true;
    }
    return llsd_equals(left, right);
}

inline bool pasteTypesCompatible(const LLSD& source, const LLSD& destination)
{
    if (!source.isDefined() || !destination.isDefined())
        return false;
    const bool source_numeric = source.isInteger() || source.isReal();
    const bool destination_numeric = destination.isInteger() || destination.isReal();
    // Vectors and colours paste only onto a row of the same component count (Vector3 onto Vector2 is refused).
    if (source.isArray() && destination.isArray() && source.size() != destination.size())
        return false;
    return source.type() == destination.type() || (source_numeric && destination_numeric);
}

inline LLSD coercePasteValue(const LLSD& source, const LLSD& destination)
{
    if (!pasteTypesCompatible(source, destination))
        return LLSD();
    if (destination.isInteger())
        return LLSD(static_cast<LLSD::Integer>(llround(source.asReal())));
    if (destination.isReal())
        return LLSD(source.asReal());
    return source;
}

inline Favorite sourceFavorite(const Favorite& row, const Favorite& source)
{
    return source.page.empty() || source.widget.empty() ? row : source;
}

inline bool isValidFavorite(const Favorite& favorite)
{
    return !favorite.page.empty() && !favorite.widget.empty() &&
           favorite.widget.rfind("quick_proxy_", 0) != 0;
}

// The projector-shaft rows moved from World > Atmosphere to Light > Projector Shafts. A favorite stored
// against the old page follows its row. Only the moved widgets are rewritten; Atmosphere keeps its own.
inline bool isProjectorShaftWidget(const std::string& widget)
{
    return widget.rfind("ps_BDMergeProjectorVolumetrics", 0) == 0 || widget == "ps_tint_swatch" ||
           widget == "ps_BDMergeFroxelWindAzimuth_slider" || widget == "ps_BDMergeFroxelWindElevation_slider" ||
           widget == "ps_BDMergeFroxelWindInverted_check" ||
           widget == "ps_BDMergeSoftShadowSoftness_slider" || widget == "ps_BDMergeSoftShadowMaxPenumbra_slider" ||
           widget == "ps_BDMergeSoftShadowFill_slider" ||
           widget.rfind("projector_volumetrics_", 0) == 0 ||
           widget == "projector_cones_enable_check" || widget == "soft_shadows_enable_check" ||
           widget == "soft_shadow_sun_check" || widget == "gobo_anisotropic_check";
}

inline Favorite migrateFavorite(Favorite favorite)
{
    if (favorite.page == "world_group/atmosphere_tab" && isProjectorShaftWidget(favorite.widget))
        favorite.page = "light_group/projector_shafts_tab";
    return favorite;
}

inline std::vector<Favorite> favoritesFromLLSD(const LLSD& value)
{
    std::vector<Favorite> result;
    if (!value.isArray())
        return result;
    for (LLSD::array_const_iterator it = value.beginArray(); it != value.endArray(); ++it)
    {
        Favorite favorite = migrateFavorite(Favorite{ (*it)["page"].asString(), (*it)["widget"].asString() });
        if (isValidFavorite(favorite) &&
            std::find(result.begin(), result.end(), favorite) == result.end())
            result.push_back(std::move(favorite));
    }
    return result;
}

inline LLSD favoritesToLLSD(const std::vector<Favorite>& favorites)
{
    LLSD result = LLSD::emptyArray();
    for (const Favorite& favorite : favorites)
    {
        LLSD item = LLSD::emptyMap();
        item["page"] = favorite.page;
        item["widget"] = favorite.widget;
        result.append(item);
    }
    return result;
}
} // namespace ALKitQol

#endif
