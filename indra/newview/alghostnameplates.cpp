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

#include "aldirectortagtargets.h"
#include "llagent.h"
#include "llfontgl.h"
#include "llhudnametag.h"
#include "llhudobject.h"
#include "llstring.h"
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

ALGhostNameplates::Entry* ALGhostNameplates::getOrCreate(const LLUUID& key)
{
    const auto found = mTags.find(key);
    if (found != mTags.end())
    {
        return &found->second;
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
    // Source-less HUD tags skip LLHUDNameTag's ordinary distance update, so
    // fading is applied explicitly in update() below.
    tag->setDoFade(false);
    tag->setFadeDistance(GHOST_NAMEPLATE_DISTANCE,
                         GHOST_NAMEPLATE_FADE_RANGE);
    tag->setZCompare(false);
    tag->setHidden(true);
    Entry entry;
    entry.mTag = tag;
    return &mTags.emplace(key, std::move(entry)).first->second;
}

void ALGhostNameplates::update(
    const std::vector<Label>& labels, bool visible)
{
    std::set<LLUUID> wanted;
    const LLVector3 camera = LLViewerCamera::getInstance()->getOrigin();
    const LLVector3 forward = LLViewerCamera::getInstance()->getAtAxis();
    ALDirectorTagTargets& targets = ALDirectorTagTargets::instance();
    const bool highlight_enabled = targets.highlightEnabled();
    static const std::string no_group;

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

            Entry* entry = getOrCreate(label.mKey);
            if (!entry || entry->mTag.isNull())
            {
                continue;
            }
            LLHUDNameTag* tag = entry->mTag.get();
            wanted.insert(label.mKey);
            const F32 fade = distance <= GHOST_NAMEPLATE_DISTANCE
                ? 1.f
                : llclamp(1.f -
                    (distance - GHOST_NAMEPLATE_DISTANCE) /
                    GHOST_NAMEPLATE_FADE_RANGE, 0.f, 1.f);
            // The yellow selected text gives way ONLY on a plate that really gets the
            // background highlight; every other selected plate keeps it exactly as before.
            const bool highlighted = highlight_enabled && label.mHighlighted;
            LLColor4 color = label.mSelected && !highlighted
                ? LLColor4(1.f, 0.82f, 0.2f, 1.f) : label.mColor;
            color.mV[VALPHA] *= fade;
            tag->setPositionAgent(label.mPositionAgent);
            // [GhostPanelPerf] setString() snapshots the current color into its text
            // segments AND re-tokenises/re-measures the text, so apply both only
            // when the text or colour changed (rename / selection / fade).
            const std::string& group_line = label.mGroupLine ? *label.mGroupLine : no_group;
            if (!entry->mApplied || entry->mText != label.mText ||
                entry->mGroupLine != group_line || entry->mColor != color)
            {
                tag->setColor(color);
                // Names and Director labels are user text: no control character or newline
                // may split a plate line. Every line uses the tag's own font, one line each
                // (ellipsis, never wrapped), and the tag's default line limit stays untouched.
                std::string name = label.mText;
                LLStringFn::replace_ascii_controlchars(name, LL_UNKNOWN_CHAR);
                tag->clearString();
                if (!group_line.empty())
                {
                    std::string group = group_line;
                    LLStringFn::replace_ascii_controlchars(group, LL_UNKNOWN_CHAR);
                    tag->addLine(group, color, LLFontGL::NORMAL, nullptr, true);
                }
                tag->addLine(name, color, LLFontGL::NORMAL, nullptr, true);
                entry->mText = label.mText;
                entry->mGroupLine = group_line;
                entry->mColor = color;
                entry->mApplied = true;
            }
            // Colour-only: a highlight change never rebuilds the text.
            switch (ALTagHighlight::overrideAction(
                        highlighted,
                        tag->hasBackgroundOverride(), tag->backgroundOverrideStamp(),
                        targets.styleStamp()))
            {
            case ALTagHighlight::OverrideAction::APPLY:
                tag->setBackgroundOverride(targets.style().background,
                                           targets.style().text, targets.styleStamp());
                break;
            case ALTagHighlight::OverrideAction::CLEAR:
                tag->clearBackgroundOverride();
                break;
            case ALTagHighlight::OverrideAction::KEEP:
                break;
            }
            tag->setHidden(false);
        }
    }

    for (auto it = mTags.begin(); it != mTags.end();)
    {
        if (wanted.find(it->first) == wanted.end())
        {
            it->second.mTag->markDead();
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
        if (entry.second.mTag.notNull())
        {
            entry.second.mTag->markDead();
        }
    }
    mTags.clear();
    mOffsets.clear();
}

void ALGhostNameplates::applyOffsets(
    const std::function<LLVector3(const LLUUID&)>& offset_for)
{
    mOffsets.clear();
    if (!offset_for) return;
    for (auto& item : mTags)
    {
        if (item.second.mTag.isNull()) continue;
        const LLVector3 delta = offset_for(item.first);
        if (delta.isExactlyZero()) continue;
        OffsetState state;
        state.mOriginal = gAgent.getPosAgentFromGlobal(item.second.mTag->getPositionGlobal());
        state.mWritten = state.mOriginal + delta;
        item.second.mTag->setPositionAgent(state.mWritten);
        mOffsets[item.first] = state;
    }
}

void ALGhostNameplates::restoreOffsets()
{
    for (const auto& item : mOffsets)
    {
        const auto tag = mTags.find(item.first);
        if (tag != mTags.end() && tag->second.mTag.notNull() &&
            gAgent.getPosAgentFromGlobal(tag->second.mTag->getPositionGlobal()) == item.second.mWritten)
            tag->second.mTag->setPositionAgent(item.second.mOriginal);
    }
    mOffsets.clear();
}
