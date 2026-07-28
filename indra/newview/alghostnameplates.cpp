/**
 * @file alghostnameplates.cpp
 * @brief Managed HUD nameplates for Ghost Studio instances and crowds.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alghostnameplates.h"

#include "llagent.h"
#include "llfontgl.h"
#include "llhudnametag.h"
#include "llhudobject.h"
#include "llviewercamera.h"

#include <set>

namespace
{
constexpr F32 GHOST_NAMEPLATE_DISTANCE = 64.f;
constexpr F32 GHOST_NAMEPLATE_FADE_RANGE = 8.f;
}

ALGhostNameplates::~ALGhostNameplates()
{
    clear();
}

LLHUDNameTag* ALGhostNameplates::getOrCreate(const LLUUID& key)
{
    const auto found = mTags.find(key);
    if (found != mTags.end())
    {
        return found->second.get();
    }

    LLPointer<LLHUDNameTag> tag = static_cast<LLHUDNameTag*>(
        LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_NAME_TAG));
    if (tag.isNull())
    {
        return nullptr;
    }
    tag->setSourceObject(nullptr);
    tag->setFont(LLFontGL::getFontSansSerif());
    tag->setTextAlignment(LLHUDNameTag::ALIGN_TEXT_CENTER);
    tag->setVertAlignment(LLHUDNameTag::ALIGN_VERT_TOP);
    tag->setVisibleOffScreen(false);
    tag->setMaxLines(1);
    // Source-less HUD tags skip LLHUDNameTag's ordinary distance update, so
    // fading is applied explicitly in update() below.
    tag->setDoFade(false);
    tag->setFadeDistance(GHOST_NAMEPLATE_DISTANCE,
                         GHOST_NAMEPLATE_FADE_RANGE);
    tag->setZCompare(false);
    tag->setHidden(true);
    mTags.emplace(key, tag);
    return tag.get();
}

void ALGhostNameplates::update(
    const std::vector<Label>& labels, bool visible)
{
    std::set<LLUUID> wanted;
    const LLVector3 camera = LLViewerCamera::getInstance()->getOrigin();
    const LLVector3 forward = LLViewerCamera::getInstance()->getAtAxis();

    if (visible)
    {
        for (const Label& label : labels)
        {
            if (label.mKey.isNull() || label.mText.empty() ||
                !label.mPositionAgent.isFinite())
            {
                continue;
            }
            const LLVector3 delta = label.mPositionAgent - camera;
            const F32 distance = delta.length();
            LLCoordGL screen;
            if (!llfinite(distance) || distance > GHOST_NAMEPLATE_DISTANCE +
                GHOST_NAMEPLATE_FADE_RANGE || delta * forward <= 0.f ||
                !LLViewerCamera::getInstance()->projectPosAgentToScreen(
                    label.mPositionAgent, screen, false))
            {
                continue;
            }

            LLHUDNameTag* tag = getOrCreate(label.mKey);
            if (!tag)
            {
                continue;
            }
            wanted.insert(label.mKey);
            const F32 fade = distance <= GHOST_NAMEPLATE_DISTANCE
                ? 1.f
                : llclamp(1.f -
                    (distance - GHOST_NAMEPLATE_DISTANCE) /
                    GHOST_NAMEPLATE_FADE_RANGE, 0.f, 1.f);
            LLColor4 color = label.mSelected
                ? LLColor4(1.f, 0.82f, 0.2f, 1.f) : label.mColor;
            color.mV[VALPHA] *= fade;
            tag->setPositionAgent(label.mPositionAgent);
            // setString() snapshots the current color into its text segments.
            tag->setColor(color);
            tag->setString(label.mText);
            tag->setHidden(false);
        }
    }

    for (auto it = mTags.begin(); it != mTags.end();)
    {
        if (wanted.find(it->first) == wanted.end())
        {
            it->second->markDead();
            it = mTags.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ALGhostNameplates::clear()
{
    for (auto& entry : mTags)
    {
        if (entry.second.notNull())
        {
            entry.second->markDead();
        }
    }
    mTags.clear();
}
