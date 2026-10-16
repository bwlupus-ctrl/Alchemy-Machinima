#ifndef AL_UNDO_VALUE_H
#define AL_UNDO_VALUE_H

#include "llcontrol.h"
#include "llsd.h"
#include "llsdutil.h"
#include <algorithm>
#include <cmath>

// Compare the setting's semantic type, rather than its LLSD storage representation.
inline bool alUndoValuesEqual(eControlType type, const LLSD& a, const LLSD& b)
{
    auto equal_real = [](const LLSD& left, const LLSD& right)
    {
        const F64 x = left.asReal(), y = right.asReal();
        return std::isfinite(x) && std::isfinite(y) &&
            std::abs(x - y) <= 1e-5 * std::max({1.0, std::abs(x), std::abs(y)});
    };
    switch (type)
    {
    case TYPE_F32: return equal_real(a, b);
    case TYPE_U32:
    case TYPE_S32: return a.asReal() == b.asReal();
    case TYPE_BOOLEAN: return a.asBoolean() == b.asBoolean();
    case TYPE_STRING: return a.asString() == b.asString();
    case TYPE_VEC3:
    case TYPE_VEC3D:
    case TYPE_QUAT:
    case TYPE_COL3:
    case TYPE_COL4:
        if (!a.isArray() || !b.isArray() || a.size() != b.size())
            return false;
        for (S32 i = 0; i < a.size(); ++i)
            if (!equal_real(a[i], b[i]))
                return false;
        return true;
    default: return a == b;
    }
}

#endif
