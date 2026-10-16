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
            std::abs(x - y) <= std::max(1e-9, 1e-5 * std::max(std::abs(x), std::abs(y)));
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

// Code-bound widgets have no saved control declaration; retain the recorded value's type.
inline bool alUndoWidgetValuesEqual(const LLSD& current, const LLSD& recorded)
{
    const eControlType type = recorded.isBoolean() ? TYPE_BOOLEAN :
        recorded.isInteger() ? TYPE_S32 : recorded.isReal() ? TYPE_F32 :
        recorded.isString() ? TYPE_STRING : recorded.isArray() ? TYPE_VEC3D : TYPE_LLSD;
    return alUndoValuesEqual(type, current, recorded);
}

#endif
