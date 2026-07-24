/**
 * @file llclientoutertransform.h
 * @brief Viewer-local post-skin transform shared by a ghost avatar and its attachments.
 */

#ifndef LL_LLCLIENTOUTERTRANSFORM_H
#define LL_LLCLIENTOUTERTRANSFORM_H

#include "llrefcount.h"
#include "m4math.h"
#include "v3math.h"

class LLClientOuterTransform final : public LLRefCount
{
public:
    LLMatrix4 mCurrent;
    LLMatrix4 mInverse;
    LLVector3 mFootPivot;
    F32 mScale = 1.f;
    U32 mRevision = 1;
    bool mEnabled = false;
};

#endif // LL_LLCLIENTOUTERTRANSFORM_H
