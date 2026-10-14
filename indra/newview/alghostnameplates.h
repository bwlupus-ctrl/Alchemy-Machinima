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
#include <functional>
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
    void applyOffsets(const std::function<LLVector3(const LLUUID&)>& offset_for);
    void restoreOffsets();

private:
    // [GhostPanelPerf] LLHUDNameTag::setString() re-tokenises and re-measures the
    // text, so remember what each tag was last given and only call setString()/
    // setColor() when the text or colour actually changed.
    struct Entry
    {
        LLPointer<LLHUDNameTag> mTag;
        std::string mText;
        LLColor4    mColor;
        bool        mApplied = false;   // mText/mColor reflect a setString() call
    };
    Entry* getOrCreate(const LLUUID& key);

    std::map<LLUUID, Entry> mTags;
    struct OffsetState { LLVector3 mOriginal, mWritten; };
    std::map<LLUUID, OffsetState> mOffsets;
};

#endif // AL_ALGHOSTNAMEPLATES_H
