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

inline std::vector<Favorite> favoritesFromLLSD(const LLSD& value)
{
    std::vector<Favorite> result;
    if (!value.isArray())
        return result;
    for (LLSD::array_const_iterator it = value.beginArray(); it != value.endArray(); ++it)
    {
        Favorite favorite{ (*it)["page"].asString(), (*it)["widget"].asString() };
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
