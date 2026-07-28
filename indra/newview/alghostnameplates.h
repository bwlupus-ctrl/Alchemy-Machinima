/**
 * @file alghostnameplates.h
 * @brief Managed HUD nameplates for Ghost Studio instances and crowds.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGHOSTNAMEPLATES_H
#define AL_ALGHOSTNAMEPLATES_H

#include "llhudnametag.h"
#include "v4color.h"
#include "llpointer.h"
#include "lluuid.h"
#include "v3math.h"

#include <map>
#include <string>
#include <vector>

class ALGhostNameplates
{
public:
    struct Label
    {
        LLUUID      mKey;
        std::string mText;
        LLVector3   mPositionAgent;
        LLColor4    mColor = LLColor4::white;
        bool        mSelected = false;
    };

    ALGhostNameplates() = default;
    ~ALGhostNameplates();

    void update(const std::vector<Label>& labels, bool visible);
    void clear();

private:
    LLHUDNameTag* getOrCreate(const LLUUID& key);

    std::map<LLUUID, LLPointer<LLHUDNameTag>> mTags;
};

#endif // AL_ALGHOSTNAMEPLATES_H
