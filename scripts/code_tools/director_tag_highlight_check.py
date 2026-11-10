"""Check the Director name-tag highlight and Director group labels without a viewer build.

Two layers, each attacked with planted negatives that MUST be rejected:

1. The pure rules in indra/newview/aldirectortagtargets.h are compiled and run against the real
   llcommon/llmath types: text contrast on every shipped theme accent (and its inverted twin),
   page -> target mapping, the opaque highlight background, the heartbeat, the per-tag override
   decision (no override may leak onto a tag that is not highlighted), and the Director group
   label cache (a dissolved/renamed/removed group may never leave a label behind).
2. Wiring: the production name-tag, clone-plate, Director, resolver and rig sources must keep the
   calls that make those rules real. Every mutation in WIRING_NEGATIVES is applied in memory and
   must make the wiring check fail.

--selftest runs only layer 2 (fast, no compiler); no argument runs everything.
Run with ordinary Python (not -I).
"""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

from native_check_tools import ROOT, check_directory, compile_run, viewer_native_flags

NV = ROOT / "indra/newview"
HEADER = NV / "aldirectortagtargets.h"


def L(*lines):
    """Join source lines; keeps the needles below free of escape sequences."""
    return "\n".join(lines)


def read(path):
    return Path(path).read_text(encoding="utf-8").replace("\r\n", "\n")


def theme_accents():
    """Every shipped KitAccent (skin + each theme file) as (r, g, b) in 0..1."""
    accents = []
    skin = read(NV / "skins/default/colors.xml")
    match = re.search(r'name="KitAccent"\s+value="([^"]+)"', skin)
    assert match, "skin KitAccent missing"
    accents.append(tuple(float(v) for v in match.group(1).split()[:3]))
    for theme in sorted((NV / "app_settings/director_themes").glob("*.xml")):
        root = ET.fromstring(read(theme))
        node = None
        elements = list(root.iter())
        for index, element in enumerate(elements):
            if element.tag == "key" and element.text == "KitAccent":
                node = elements[index + 1]
                break
        if node is None:
            continue    # the default theme defers to the skin colour read above
        assert node.tag == "array", theme.name
        accents.append(tuple(float(real.text) for real in node.findall("real")[:3]))
    return accents


def fixture(accents):
    accent_rows = ",".join("{%.6ff,%.6ff,%.6ff}" % accent for accent in accents)
    return r'''
#include "linden_common.h"
#include "aldirectortagtargets.h"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
using namespace ALTagHighlight;

static void check(bool ok, const char* what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; std::exit(1); }
}

int main()
{
    // ---- 1. Readable text: >= 4.5:1 on every shipped accent, its inverted twin, and a colour sweep ----
    const float accents[][3] = { ''' + accent_rows + r''' };
    std::vector<LLColor4> sweep;
    for (const auto& a : accents)
    {
        sweep.emplace_back(a[0], a[1], a[2], 1.f);
        sweep.push_back(ALDirectorTheme::invertedColor(sweep.back()));
    }
    for (int r = 0; r <= 10; ++r) for (int g = 0; g <= 10; ++g) for (int b = 0; b <= 10; ++b)
        sweep.emplace_back(r / 10.f, g / 10.f, b / 10.f, 1.f);
    for (const LLColor4& accent : sweep)
    {
        const TagStyle style = tagStyleFor(accent);
        check(ALDirectorTheme::contrastRatio(style.text, style.background) >= 4.5f,
              "highlight text contrast below 4.5:1");
        check(style.background.mV[VALPHA] == 1.f, "highlight background must be opaque accent");
        check(style.background.mV[0] == accent.mV[0] && style.background.mV[2] == accent.mV[2],
              "highlight background must be the accent itself");
        check(styleReadable(style), "styleReadable disagrees");
    }
    // a style that is NOT readable must be reported as such (the judge itself is not a rubber stamp)
    TagStyle bad;
    bad.background = LLColor4(0.5f, 0.5f, 0.5f, 1.f);
    bad.text = LLColor4(0.55f, 0.55f, 0.55f, 1.f);
    check(!styleReadable(bad), "styleReadable accepted unreadable text");

    // ---- 2. Pages: the page decides who is edited, and only actor-driven pages highlight ----
    check(pageTarget("performance_group/gaze_tab") == PageTarget::GAZE, "gaze page uses its effective edit actors");
    check(pageTarget("performance_group/gaze_cues_tab") == PageTarget::ACTOR_BAR, "gaze cues follow the Actor bar");
    check(pageTarget("cast_group/ghosts_tab") == PageTarget::CLONES, "clones page");
    check(pageTarget("cast_group/groups_tab") == PageTarget::MANAGED_GROUP, "groups page");
    check(pageTarget("cast_group/move_tab") == PageTarget::MOVE, "move page uses its Walk/Stop targets");
    check(pageTarget("light_group/cine_light_rig_tab") == PageTarget::LIGHT_RIG, "rig page");
    check(pageTarget("light_group/actors_tab") == PageTarget::LIGHT_ACTORS, "light actors page uses its list selection");
    check(pageTarget("look_group/stylize_tab") == PageTarget::ROTO, "Roto Ink acts on its own subject / set");
    check(pageTarget("look_group/tron_tab") == PageTarget::TRON, "Tron acts on its own subject / set");
    check(pageTarget("world_group/shadows_probes_tab") == PageTarget::SHADOWS, "Shadows follow their own subject");
    check(pageTarget("look_group/actor_style_tab") == PageTarget::ACTOR_BAR, "Actor FX follows the Actor bar");
    check(pageTarget("performance_group/switchboard_tab") == PageTarget::NONE, "switchboard targets per slot");
    check(pageTarget("settings_group/director_settings_tab") == PageTarget::NONE, "settings acts on nobody");
    check(pageTarget("camera_group/shot_tab") == PageTarget::NONE, "camera page acts on nobody");
    check(pageTarget("") == PageTarget::NONE, "no page, no target");

    // ---- 3. Highlight opacity: fully opaque, fading only with the tag itself ----
    check(highlightedBackgroundAlpha(1.0f) == 1.0f, "an opaque tag stays opaque (no bubble-opacity cap)");
    check(highlightedBackgroundAlpha(0.4f) == 0.4f, "the highlight fades with the tag's own fade");
    check(highlightedBackgroundAlpha(2.0f) == 1.0f && highlightedBackgroundAlpha(-1.0f) == 0.0f, "fade is clamped");

    // ---- 4. Per-tag override decision: no leak onto a tag that is not highlighted ----
    struct Tag { bool has = false; U64 stamp = 0; };
    const int kTags = 4;
    Tag tags[kTags];
    U64 style_stamp = 1;
    auto frame = [&](const bool wanted[kTags])
    {
        for (int i = 0; i < kTags; ++i)
        {
            switch (overrideAction(wanted[i], tags[i].has, tags[i].stamp, style_stamp))
            {
            case OverrideAction::APPLY: tags[i].has = true; tags[i].stamp = style_stamp; break;
            case OverrideAction::CLEAR: tags[i].has = false; tags[i].stamp = 0; break;
            case OverrideAction::KEEP: break;
            }
            check(tags[i].has == wanted[i], "override present on a tag that is not highlighted (or missing on one that is)");
        }
    };
    const bool a[kTags] = { true, false, true, false };
    const bool b[kTags] = { false, true, false, false };
    const bool none[kTags] = { false, false, false, false };
    frame(a); frame(a); frame(b); frame(none); frame(a);
    style_stamp = 2;    // live theme / Invert change: highlighted tags re-apply, others stay clean
    frame(a);
    check(tags[0].stamp == 2 && tags[2].stamp == 2, "style change must reach highlighted tags");
    frame(none);
    check(overrideAction(false, false, 0, 7) == OverrideAction::KEEP, "idle tag must not be touched");
    check(overrideAction(true, true, 7, 7) == OverrideAction::KEEP, "unchanged highlight must not be re-applied");

    // ---- 5. Heartbeat: time window OR the last few frames; withdrawn or stale never shows ----
    check(heartbeatFresh(10.2, 10.0, 100, 99), "recent beat is fresh");
    check(!heartbeatFresh(10.9, 10.0, 130, 100), "stale beat must not highlight");
    check(heartbeatFresh(14.0, 10.0, 101, 100), "one very long frame must not expire a drawing Director");
    check(!heartbeatFresh(10.0, 0.0, 100, 100), "never published / withdrawn must not highlight");

    // ---- 5b. Renderer records: only a record refreshed in the last few frames counts ----
    check(recordFresh(true, 100, 99), "a record from last frame is current");
    check(!recordFresh(true, 110, 100), "a stale record (effect no longer rendering) must publish nobody");
    check(!recordFresh(false, 100, 100), "a never-recorded effect must publish nobody");

    // ---- 6. Id set: replaced, not merged; nulls dropped ----
    IdSet set;
    LLUUID one, two, three;
    one.generate(); two.generate(); three.generate();
    std::vector<LLUUID> ids = { one, two, LLUUID::null, one };
    check(set.assign(ids), "first assign changes");
    check(set.contains(one) && set.contains(two) && !set.contains(LLUUID::null), "membership / nulls");
    check(set.size() == 2, "nulls and duplicates must be dropped from the set");
    ids = { three };
    check(set.assign(ids), "second assign changes");
    check(!set.contains(one) && !set.contains(two) && set.contains(three), "old ids must not survive a replace");
    ids = { three };
    check(!set.assign(ids), "identical assign reports no change");
    check(set.clear() && !set.contains(three), "clear empties");

    // ---- 7. Director group labels follow the cast and groups exactly ----
    std::map<LLUUID, std::string> cast;     // id -> group tag, the Director's truth
    U64 cast_rev = 1, group_rev = 1;
    GroupLabelCache cache;
    auto visit = [&](auto add) { for (const auto& m : cast) add(m.first, m.second); };
    auto refresh = [&]() { cache.refresh(cast_rev, group_rev, visit); };
    cast[one] = "Crowd"; cast[two] = "Crowd"; cast[three] = "";
    refresh();
    check(cache.label(one) == "Crowd" && cache.label(two) == "Crowd", "member shows its Director group");
    check(cache.label(three).empty(), "ungrouped member shows no label");
    // rename
    cast[one] = cast[two] = "Guards"; ++group_rev;
    refresh();
    check(cache.label(one) == "Guards", "rename reaches the label");
    // one member removed from the group
    cast[two] = ""; ++group_rev;
    refresh();
    check(cache.label(two).empty() && cache.label(one) == "Guards", "leaving a group drops only that label");
    // membership change that bumps only the cast revision (member removed from the cast)
    cast.erase(one); ++cast_rev;
    refresh();
    check(cache.label(one).empty(), "removed cast member keeps no label");
    // dissolve: every tag cleared. Only the group revision moves (a defensive case).
    cast[one] = "Guards"; cast[two] = "Guards"; ++cast_rev; ++group_rev; refresh();
    check(cache.label(one) == "Guards", "setup");
    cast[one] = ""; cast[two] = ""; ++group_rev;
    refresh();
    check(cache.label(one).empty() && cache.label(two).empty(), "dissolved group still shown on a tag");
    // the cast disappears altogether
    cast[one] = "Guards"; ++cast_rev; ++group_rev; refresh();
    check(cache.label(one) == "Guards", "setup 2");
    cache.invalidate();
    check(cache.label(one).empty(), "label survived the cast going away");
    const U64 revision = cache.revision();
    refresh();
    check(cache.revision() != revision, "a rebuild after invalidate must be visible to tags");
    U64 unchanged = cache.revision();
    refresh();
    check(cache.revision() == unchanged, "no revision change, no rebuild");

    std::cout << "PASS: contrast " << sweep.size() << " accents, pages, opacity, override decisions, heartbeat, id set, group labels\n";
    return 0;
}
'''


def with_header(directory, header_text):
    (directory / "aldirectortagtargets.h").write_text(header_text, encoding="utf-8")


def run_fixture(directory, flags, libraries, header_text, name, accents):
    with_header(directory, header_text)
    local = [f'/I"{directory}"'] + flags
    try:
        return compile_run(directory, fixture(accents), name, flags=local, libraries=libraries)
    except RuntimeError as error:
        raise RuntimeError("fixture failed to compile for " + name + "\n" + str(error)[:3000])


HEADER_NEGATIVES = {
    "hardcoded_white_text": (
        L("return ALDirectorTheme::contrastRatio(black, background) >=",
          "               ALDirectorTheme::contrastRatio(white, background) ? black : white;"),
        "(void)black; return white;"),
    "text_is_accent": (
        "style.text = readableTextOn(style.background);",
        "style.text = style.background;"),
    "translucent_background": (
        "style.background.mV[VALPHA] = 1.f;",
        "style.background.mV[VALPHA] = accent.mV[VALPHA] * 0.4f;"),
    "judge_rubber_stamp": (
        "return ALDirectorTheme::contrastRatio(style.text, style.background) >= kMinTextContrast;",
        "return true;"),
    "everything_acts_on_actor_bar": (
        L("        return PageTarget::NONE;", "    }", "", "    inline constexpr F32 kMinTextContrast"),
        L("        return PageTarget::ACTOR_BAR;", "    }", "", "    inline constexpr F32 kMinTextContrast")),
    "roto_targetless": (
        'PageTarget::ROTO },', 'PageTarget::NONE },'),
    "faint_highlight": (
        "return std::clamp(tag_fade, 0.f, 1.f);",
        "return std::clamp(tag_fade, 0.f, 0.5f);"),
    "override_leaks": (
        "return has_override ? OverrideAction::CLEAR : OverrideAction::KEEP;",
        "return OverrideAction::KEEP;"),
    "override_ignores_theme_change": (
        "return !has_override || applied_stamp != style_stamp ? OverrideAction::APPLY",
        "return !has_override ? OverrideAction::APPLY"),
    "override_reapplied_every_frame": (
        L("return !has_override || applied_stamp != style_stamp ? OverrideAction::APPLY",
          "                                                              : OverrideAction::KEEP;"),
        "return OverrideAction::APPLY;"),
    "stale_heartbeat_lingers": (
        "return (now >= beat && now - beat <= kHeartbeatWindow) ||",
        "return true ||"),
    "long_frame_expires_heartbeat": (
        "static_cast<U32>(frame - beat_frame) <= kHeartbeatFrames;",
        "false;"),
    "withdrawn_heartbeat_lingers": (
        "return false;   // never published, or withdrawn",
        "return true;"),
    "stale_record_lingers": (
        "return recorded && static_cast<U32>(frame - record_frame) <= kRecordFrames;",
        "return recorded;"),
    "unrecorded_record_shown": (
        "return recorded && static_cast<U32>(frame - record_frame) <= kRecordFrames;",
        "return static_cast<U32>(frame - record_frame) <= kRecordFrames;"),
    "id_set_merges": (
        L("mIds.swap(ids);", "            return true;"),
        L("mIds.insert(mIds.end(), ids.begin(), ids.end());",
          "            std::sort(mIds.begin(), mIds.end());", "            return true;")),
    "id_set_keeps_null": (
        "[](const LLUUID& id) { return id.isNull(); }), ids.end());",
        "[](const LLUUID&) { return false; }), ids.end());"),
    "label_ignores_group_revision": (
        "if (mValid && cast_revision == mCastRevision && group_revision == mGroupRevision)",
        "if (mValid && cast_revision == mCastRevision)"),
    "label_ignores_cast_revision": (
        "if (mValid && cast_revision == mCastRevision && group_revision == mGroupRevision)",
        "if (mValid && group_revision == mGroupRevision)"),
    "label_rebuild_keeps_old": (
        L("mLabels.clear();", "            visit("),
        "visit("),
    "label_survives_cast_loss": (
        L("const bool had = mValid || !mLabels.empty();", "            mLabels.clear();"),
        "const bool had = mValid || !mLabels.empty();"),
    "label_without_revision_bump": (
        L("mValid = true;", "            ++mRevision;"),
        "mValid = true;"),
}


# ---- wiring: production sources must keep the calls that make the rules real ----
def wiring_sources():
    return {
        "nametag": read(NV / "llhudnametag.cpp"),
        "nametag_h": read(NV / "llhudnametag.h"),
        "avatar": read(NV / "llvoavatar.cpp"),
        "plates": read(NV / "alghostnameplates.cpp"),
        "studio": read(NV / "alghoststudio.cpp"),
        "director": read(NV / "llfloaterdirector.cpp"),
        "resolver": read(NV / "aldirectortagtargets.cpp"),
        "rig": read(NV / "alcinelightrig.cpp"),
        "mover": read(NV / "alpanelactormover.cpp"),
        "pipeline": read(NV / "pipeline.cpp"),
        "tron": read(NV / "altron.cpp"),
        "settings": read(NV / "app_settings/settings.xml"),
    }


SINGLE_LOOKUP = L("    return !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&",
                  "        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,",
                  "                                       LLFrameTimer::getFrameCount(), mBeatFrame) &&",
                  "        mIds.contains(id);")
DOUBLE_LOOKUP = L("    return !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&",
                  "        ALTagHighlight::heartbeatFresh(LLFrameTimer::getElapsedSeconds(), mBeat,",
                  "                                       LLFrameTimer::getFrameCount(), mBeatFrame) &&",
                  "        (mIds.contains(first) || mIds.contains(second));")
RENDER_OVERRIDE = L("    if (use_override)", "    {", "        // Director highlight: the override colour, fully opaque (the text contrast guarantee")
PLATE_COLOR = "            LLColor4 color = label.mSelected && !highlighted"
ACTOR_BAR_CASE = "        add_actor(cast.getActiveActorId());"
CLONE_COLLECT = "            host->appendSelectedInstances(mTagSelectionScratch);"


def wiring_errors(src):
    errors = []

    def need(key, needle, why):
        if needle not in src[key]:
            errors.append(f"{key}: {why}")

    def forbid(key, needle, why):
        if needle in src[key]:
            errors.append(f"{key}: {why}")

    def ordered(key, first, second, why):
        text = src[key]
        a, b = text.find(first), text.find(second)
        if a < 0 or b < 0 or a > b:
            errors.append(f"{key}: {why}")

    # A tag without an override draws exactly as before: the global background is computed first,
    # the override only replaces it behind the flag, fully opaque, text and label recoloured.
    ordered("nametag", "bg_color.setAlpha(bubble_opacity * alpha_factor);", "if (use_override)",
            "the override must sit behind the unchanged global background")
    need("nametag", RENDER_OVERRIDE, "the background override must be conditional on the per-tag flag")
    need("nametag", "bg_color = mBackgroundOverride;", "override colour is used")
    need("nametag", "bg_color.setAlpha(ALTagHighlight::highlightedBackgroundAlpha(alpha_factor * mColor.mV[VALPHA]));",
         "highlight is opaque and fades only with the tag")
    forbid("nametag", "highlightedBackgroundAlpha(bubble_opacity", "no ChatBubbleOpacity cap on the highlight")
    need("nametag", "text_color.mV[channel] = mBackgroundOverrideText.mV[channel];", "text colour flips with the highlight")
    need("nametag", "label_color.mV[channel] = mBackgroundOverrideText.mV[channel];", "label text flips with the highlight")
    need("nametag", "mHasBackgroundOverride(false)", "tags start without an override")
    need("nametag", L("    const bool use_override = mHasBackgroundOverride &&",
                      "        ALDirectorTagTargets::instance().overrideActive(mBackgroundOverrideStamp);"),
         "an override draws only while the Director's published set is active and its style is current")
    need("resolver", "return stamp == mStyleStamp && !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&",
         "override validity: current style, active set, setting, session")
    need("resolver", L("bool ALDirectorTagTargets::overrideActive(U64 stamp) const", "{",
                       "    return stamp == mStyleStamp && !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&",
                       "        ALTagHighlight::heartbeatFresh("), "override validity includes the heartbeat")
    need("nametag", "void LLHUDNameTag::clearBackgroundOverride()", "clear exists")
    # Real avatars: the decision goes through overrideAction, so a non-highlighted tag is always cleared.
    need("avatar", "ALTagHighlight::overrideAction(wanted, mNameText->hasBackgroundOverride()", "avatar tag uses the shared decision")
    need("avatar", L("case ALTagHighlight::OverrideAction::CLEAR:", "        mNameText->clearBackgroundOverride();"),
         "avatar tag clears a stale override")
    need("avatar", L("idleUpdateNameTagText(new_name);", "    idleUpdateNameTagAlpha(new_name, alpha);",
                     "    idleUpdateNameTagHighlight();"), "highlight runs after the text, colour-only (no text rebuild)")
    # Director group label replaces the SL title, rebuilds when it changes, the SL title returns,
    # and RLV name hiding hides it.
    need("avatar", L("    const std::string& director_group = (fRlvShowAvName || isSelf())",
                     "        ? ALDirectorTagTargets::instance().groupLabel(getID()) : no_director_group;"),
         "avatar asks the resolver, never on an RLV-hidden name")
    ordered("avatar", "bool fRlvShowAvName = RlvActions::canShowName(RlvActions::SNC_DEFAULT, getID());",
            "const std::string& director_group = (fRlvShowAvName || isSelf())",
            "the RLV decision is made before the Director label is chosen")
    need("avatar", "|| director_group != mNameDirectorGroup", "a changed Director group rebuilds the tag")
    need("avatar", "mNameDirectorGroup = director_group;", "built label is remembered")
    ordered("avatar", "if (!director_group.empty())", "else if (render_title && fRlvShowAvName",
            "the SL group title is shown only when there is no Director group")
    # Clone plates: yellow gives way only to a real highlight; no line limits; sanitised; no copies.
    need("plates", "const bool highlighted = highlight_enabled && label.mHighlighted;", "highlight state is per plate")
    need("plates", PLATE_COLOR, "yellow text gives way only on a plate that gets the highlight")
    need("plates", L("case ALTagHighlight::OverrideAction::CLEAR:", "                tag->clearBackgroundOverride();"),
         "clone plate clears a stale override")
    need("plates", "entry->mGroupLine != group_line", "a changed Director group rebuilds the plate text")
    forbid("plates", "setMaxLines(", "plates keep the tag's default line limit")
    need("plates", "LLStringFn::replace_ascii_controlchars(name, LL_UNKNOWN_CHAR);", "plate names are sanitised")
    need("plates", "LLStringFn::replace_ascii_controlchars(group, LL_UNKNOWN_CHAR);", "plate group lines are sanitised")
    forbid("plates", "LLFontGL::NORMAL, LLFontGL::getFont", "plate lines use the tag's own font")
    need("studio", "member_label.mGroupLine = &tag_targets.groupLabel(inst->mEntityId, inst->mId);", "grouped clone plates carry the group")
    need("studio", "label.mGroupLine = &tag_targets.groupLabel(inst.mEntityId, inst.mId);", "clone plates carry the group")
    need("studio", "label.mGroupLine = shared_group_ok ? shared_group : nullptr;", "crowd plates show only a shared Director group")
    need("studio", "label.mHighlighted = tag_targets.isHighlighted(inst.mId, inst.mEntityId);", "clone plates ask the resolver")
    # Director: v2 owner only; effective targets; lifecycle; live theme accent.
    need("director", "ALDirectorTagTargets::instance().withdraw(this);\n    if (mShell) mShell->closeActorChooser();", "withdraw on close")
    need("director", L("if (minimized)", "        ALDirectorTagTargets::instance().withdraw(this);"), "withdraw on minimize")
    need("director", L("    if (!mIsV2 || !mShell)", "    {", "        return;", "    }"),
         "a v1 or shell-less Director never touches the highlight")
    need("director", L("    if (isMinimized() || !getVisible() || !targets.highlightEnabled())", "    {",
                       "        targets.withdraw(this);"), "withdraw when hidden, minimized or the setting is off")
    need("director", "targets.publish(this, ids, ALKitStyle::color(ALKitStyle::ACCENT).get());", "owner publishes the live theme accent")
    need("director", "ALTagHighlight::pageTarget(page->pageKey())", "page decides the target")
    need("director", "ALDirectorTagTargets::canonicalId(id)", "self is normalised in one place")
    need("director", ACTOR_BAR_CASE, "the Actor bar's chosen actor is published")
    need("director", "for (const LLUUID& id : mGazePanel->editActors()) add_actor(id);", "Gaze publishes its effective edit actors")
    need("director", "for (const LLUUID& id : mMoverPanel->effectiveTargets()) add_actor(id);", "Move publishes its Walk/Stop targets")
    need("director", CLONE_COLLECT, "clone targets come from the Director's own Clones panel")
    need("director", "hostPanelOf<ALPanelGhostStudio>(page)", "the Clones panel is this Director's own")
    need("director", "for (const LLUUID& id : panel->selectedKeys()) add_actor(id);", "Light > Actors publishes its list selection")
    need("director", "ALDirectorTagTargets::appendSubjectTargets(kind, mTagSelectionScratch);", "Roto / Tron / Shadows publish their subjects")
    need("director", "const ALCineLightRig& rig = rigs.selected();", "rig reference bound before iterating")
    need("director", "    refreshStatusStrip();\n    refreshTagHighlight();", "published from the Director draw")
    # Resolver.
    need("resolver", "mGroups.refresh(cast.getCastRevision(), cast.getGroupRevision()", "labels follow both revisions")
    need("resolver", "const LLUUID id = ALDirectorTagTargets::canonicalId(member.mId);", "labels use the shared self identity")
    need("resolver", "mGroups.invalidate();", "no cast, no labels")
    need("resolver", SINGLE_LOOKUP, "single-id lookup: toggle, session and heartbeat gate the highlight")
    need("resolver", DOUBLE_LOOKUP, "two-id lookup: toggle, session and heartbeat gate the highlight")
    need("resolver", L("    if (owner != mOwner)", "    {"), "only the publishing Director may withdraw")
    need("resolver", L("    if (!sessionAllowsHighlight())", "    {", "        withdraw(owner);"),
         "no publication while logged out, disconnected or teleporting")
    need("resolver", "gAgent.getTeleportState() == LLAgent::TELEPORT_NONE", "teleport suppresses the highlight")
    # Roto / Tron read what the renderer RECORDED; no selection runs from UI code.
    forbid("resolver", "gatherRotoInkTargets(", "no gather from UI draw")
    forbid("resolver", "collectRotoInkCandidates(", "no candidate collection from UI draw")
    forbid("resolver", "gGLLast", "no render matrices read from UI draw")
    forbid("resolver", "resolveSubject", "no name-refreshing resolve from UI draw")
    need("resolver", "cast.resolve(subject, false)", "Shadows resolve without the name refresh")
    need("resolver", L("        const LLPipeline::DirectorTagRecord& record = gPipeline.getRotoInkTagRecord();",
                       "        if (ALTagHighlight::recordFresh(record.mRecorded, LLFrameTimer::getFrameCount(), record.mFrame))"),
         "Roto publishes its fresh render record only")
    need("resolver", "if (ALTagHighlight::recordFresh(mask.mRecorded, frame, mask.mFrame))", "Tron subject record must be fresh")
    need("resolver", "if (trails && ALTagHighlight::recordFresh(trail_recorded, frame, trail_frame))", "Tron trail record must be fresh")
    need("resolver", "out.insert(out.end(), record.mIds.begin(), record.mIds.end());", "Roto record ids are published")
    need("resolver", "out.insert(out.end(), mask.mIds.begin(), mask.mIds.end());", "Tron subject record ids are published")
    need("resolver", "out.insert(out.end(), trails->begin(), trails->end());", "Tron trail record ids are published")
    need("pipeline", L("        if (out_selected_ids)", "        {",
                       "            out_selected_ids->push_back(c.mId);     // Director tag highlight: exactly the targets output",
                       "        }", "        ++count;"), "the gather records exactly the targets it outputs")
    forbid("pipeline", L("        for (const Culled& c : culled)", "        {", "            out_selected_ids->push_back(c.mId);"),
           "no recording before smoothing / projection")
    need("pipeline", L("", "    const bool tag_view = isTagRecordView();"), "Roto records on the main view (incl. the sensor overlay pass)")
    forbid("pipeline", "isTagRecordView() && !sensor_overlay", "the sensor overlay call is the main-view Roto output")
    need("pipeline", L("", "        const bool tag_view = isTagRecordView();"), "Tron records on the main view only")
    need("pipeline", L("bool LLPipeline::isTagRecordView() const", "{",
                       "    return mRT == &mMainRT && !gSnapshot && !gCubeSnapshot && !gSnapshotNoPost &&",
                       "        !sRenderingHUDs && !sImpostorRender && !sPrismLensRender && !sPrismPreviewRender;"),
         "main-view provenance predicate")
    need("pipeline", "tag_view && mask_consumed ? &mTronSubjectTagRecord.mIds : nullptr);", "Tron mask ids only where the mask is consumed")
    need("pipeline", L("        if (tag_view && anchors_consumed)", "        {",
                       "            for (S32 i = 0; i < anchor_count; ++i)", "            {",
                       "                mTronSubjectTagRecord.mIds.push_back(candidates[i].mId);"), "Tron radius anchors are recorded")
    need("pipeline", "tag_view ? &mRotoInkTagRecord.mIds : nullptr);", "Roto target-set records on the main view only")
    need("tron", L("            sTrailTagFrame = LLFrameTimer::getFrameCount();", "        }", "        return; // no emission"),
         "paused trails that are still drawn keep their record")
    need("tron", L("                sHasGeometry = true;", "                sTrailTagIds.push_back(kv.first);"),
         "paused trails publish exactly the retained drawable histories")
    need("tron", L("        if (trailDrawable(kv.second))", "        {", "            sTrailTagIds.push_back(kv.first);"),
         "running trails also publish the owners of histories still drawn")
    ordered("tron", "sTrails.erase(not_in_set[i]);", L("    sTrailTagIds.clear();", "    for (const auto& cand : cands)"),
            "the running trail record is taken after the eviction")
    need("pipeline", "(grade_strength > 0.f && grade_keep_subject > 0.f) ||", "Tron mask: grade keep-subject needs the grade on")
    need("pipeline", "(surface_consumer && grid_subject_exclude > 0.f) || rim_on;", "Tron mask: exclude needs an active grid / trace")
    need("pipeline", "const bool surface_consumer = (grid_on && grid_intensity > 0.f) || trace_on;", "Tron surface consumers")
    need("pipeline", "const bool anchors_consumed = surface_consumer && grid_subject_radius > 0.f;", "Tron anchors need an active surface")
    need("pipeline", L("            if (raw_center.isFinite() && raw_half.isFinite())", "            {",
                       "                if (tag_view && (subject_mode == 1"), "Roto records the subject only where its extents are valid")
    need("director", L("void LLFloaterDirector::setVisible(bool visible)", "{", "    if (!visible)",
                       "        ALDirectorTagTargets::instance().withdraw(this);"), "withdraw when hidden")
    need("director", "if (rigs.isSlotLit(rigs.selectedSlot()))", "an off or non-lighting rig publishes nobody")
    need("resolver", "gViewerWindow && gViewerWindow->getUIVisibility() && !gAgentCamera.cameraMouselook();",
         "no highlight with the UI hidden or in mouselook")
    need("studio", L("            if (!nameplateAnchor(*inst, member_anchor))", "            {", "                continue;", "            }",
                     "            // Only a member that really contributes to the crowd plate counts for its highlight"),
         "crowd plate votes only for members that get a plate")
    need("pipeline", "tag_view ? &mRotoInkTagRecord.mIds : nullptr);", "the Roto target-set gather records its selection")
    need("pipeline", "mRotoInkTagRecord.mIds.push_back(avatar->getID());", "Roto records the subject it really used")
    need("pipeline", "tag_view && mask_consumed ? &mTronSubjectTagRecord.mIds : nullptr);", "the Tron subject gather records its selection")
    need("pipeline", "mTronSubjectTagRecord.mFrame = LLFrameTimer::getFrameCount();", "Tron records its frame")
    need("pipeline", "mRotoInkTagRecord.mFrame = LLFrameTimer::getFrameCount();", "Roto records its frame")
    forbid("pipeline", "selection_only", "the renderer has no UI selection mode")
    need("tron", "sTrailTagIds.push_back(cand.mId);", "the trail sampler records its selection")
    need("tron", "sTrailTagFrame = LLFrameTimer::getFrameCount();", "the trail sampler records its frame")
    forbid("tron", "appendTrailActorIds", "no UI trail selection")
    # Rig: a complete, cleared resolved-member snapshot for every target mode.
    need("rig", L("        // An object target lights no avatar: a failed resolution must not keep an earlier snapshot.",
                  "        mLastResolvedMembers.clear();"), "object targets clear the snapshot")
    need("rig", L("    if (target == LitTarget::LEGACY)", "    {",
                  "        // Legacy slot groups never filled the identity snapshot; publish who this rig really lights.",
                  "        // The named-group fields stay consistent: no stored identities, none counted as present.",
                  "        mLastResolvedMembers.clear();", "        mLastResolvedStoredMembers.clear();",
                  "        mLitGroupPresent = 0;"), "legacy groups fill the snapshot and clear every field consistently")
    need("mover", "uuid_vec_t ALPanelActorMover::effectiveTargets() const", "Move exposes its effective targets")
    need("director", L("        mTagSelectionScratch.clear();",
                       "        ALDirectorTagTargets::appendSubjectTargets(kind, mTagSelectionScratch);",
                       "        for (const LLUUID& id : mTagSelectionScratch) add_actor(id);"),
         "look page targets go through add_actor")
    need("director", "targets.touch(this);", "the heartbeat follows every draw")
    need("director", L("~LLFloaterDirector()", "{", "    ALAvatarSubjectPicker::cancelFor(this);",
                       "    // Destroyed without onClose (LLFloaterReg::destroyInstance): never leave a highlight behind.",
                       "    ALDirectorTagTargets::instance().withdraw(this);"), "destructor withdraws")
    need("director", L("        const std::string group = selectedManagedGroup();", "        if (!group.empty())"),
         "no group selected means nobody")
    need("resolver", "    mOwner = nullptr;", "withdraw releases the owner")
    need("resolver", L("    if (mOwner && mOwner != owner &&"), "sticky owner")
    need("resolver", L("    if (id.isNull())", "    {", "        return gAgentID;"), "null is You")
    need("resolver", "add(clone->mEntityId, member.mGroup);", "labels carry every clone identity")
    need("resolver", '#include "llappviewer.h"', "explicit app header")
    need("avatar", "(isSelf() || RlvActions::canShowName(RlvActions::SNC_DEFAULT, getID()));", "highlight respects RLV name hiding")
    need("nametag", "label_top_color = mBackgroundOverride;", "the label strip follows the highlight")
    need("plates", '#include "llstring.h"', "explicit string header")
    need("rig", "            if (group_members[i]) mLastResolvedMembers.push_back(group_members[i]->getID());",
         "legacy members are pushed")
    need("mover", L("        return everyone;"), "Move returns the whole roster for Everyone")
    need("mover", L("    return selectedTargets();", "}", "", "void ALPanelActorMover::onClickWalk()"),
         "Move returns the selected / pinned targets")
    need("settings", "<key>DirectorTagHighlight</key>", "setting declared")
    return errors


WIRING_NEGATIVES = {
    "override_before_background": ("nametag",
        "bg_color.setAlpha(bubble_opacity * alpha_factor);", "(void)alpha_factor;"),
    "renderer_override_unconditional": ("nametag",
        RENDER_OVERRIDE, RENDER_OVERRIDE.replace("if (use_override)", "if (true)")),
    "renderer_highlight_capped_by_bubble_opacity": ("nametag",
        "bg_color.setAlpha(ALTagHighlight::highlightedBackgroundAlpha(alpha_factor * mColor.mV[VALPHA]));",
        "bg_color.setAlpha(ALTagHighlight::highlightedBackgroundAlpha(bubble_opacity * alpha_factor * mColor.mV[VALPHA]));"),
    "text_not_flipped": ("nametag",
        "text_color.mV[channel] = mBackgroundOverrideText.mV[channel];", "(void)channel;"),
    "label_not_flipped": ("nametag",
        "label_color.mV[channel] = mBackgroundOverrideText.mV[channel];", "(void)channel;"),
    "avatar_never_clears": ("avatar",
        L("case ALTagHighlight::OverrideAction::CLEAR:", "        mNameText->clearBackgroundOverride();"),
        L("case ALTagHighlight::OverrideAction::CLEAR:", "        (void)0;")),
    "avatar_group_not_rebuilt": ("avatar", "|| director_group != mNameDirectorGroup", ""),
    "avatar_title_shown_with_group": ("avatar", "else if (render_title && fRlvShowAvName", "if (render_title && fRlvShowAvName"),
    "avatar_group_ignores_rlv": ("avatar",
        "const std::string& director_group = (fRlvShowAvName || isSelf())",
        "const std::string& director_group = (true)"),
    "yellow_resuppressed_by_setting": ("plates", PLATE_COLOR,
        "            LLColor4 color = label.mSelected && !highlight_enabled"),
    "plates_never_clear": ("plates",
        L("case ALTagHighlight::OverrideAction::CLEAR:", "                tag->clearBackgroundOverride();"),
        L("case ALTagHighlight::OverrideAction::CLEAR:", "                (void)0;")),
    "plates_group_not_rebuilt": ("plates", "entry->mGroupLine != group_line", "false"),
    "plates_cap_lines": ("plates",
        "                tag->clearString();",
        "                tag->setMaxLines(2);\n                tag->clearString();"),
    "plates_unsanitised_name": ("plates",
        "LLStringFn::replace_ascii_controlchars(name, LL_UNKNOWN_CHAR);", "(void)name;"),
    "plates_hardcoded_font": ("plates",
        "tag->addLine(name, color, LLFontGL::NORMAL, nullptr, true);",
        "tag->addLine(name, color, LLFontGL::NORMAL, LLFontGL::getFontSansSerif(), true);"),
    "crowd_plate_shows_mixed_group": ("studio",
        "label.mGroupLine = shared_group_ok ? shared_group : nullptr;", "label.mGroupLine = shared_group;"),
    "director_no_withdraw_on_close": ("director",
        "ALDirectorTagTargets::instance().withdraw(this);\n    if (mShell) mShell->closeActorChooser();",
        "if (mShell) mShell->closeActorChooser();"),
    "director_v1_withdraws": ("director",
        L("    if (!mIsV2 || !mShell)", "    {", "        return;", "    }"),
        L("    if (!mIsV2 || !mShell)", "    {", "        ALDirectorTagTargets::instance().withdraw(nullptr);", "        return;", "    }")),
    "director_hardcoded_accent": ("director",
        "targets.publish(this, ids, ALKitStyle::color(ALKitStyle::ACCENT).get());",
        "targets.publish(this, ids, LLColor4(0.91f, 0.64f, 0.24f, 1.f));"),
    "publish_self_instead_of_actor": ("director", ACTOR_BAR_CASE, "        add_actor(gAgentID);"),
    "clone_target_collection_removed": ("director", CLONE_COLLECT, "            (void)host;"),
    "clones_from_global_host": ("director",
        "hostPanelOf<ALPanelGhostStudio>(page)", "ALPanelGhostStudio::activeHost()"),
    "gaze_uses_actor_bar_only": ("director",
        "for (const LLUUID& id : mGazePanel->editActors()) add_actor(id);", "add_actor(cast.getActiveActorId());"),
    "move_ignores_everyone_scope": ("director",
        "for (const LLUUID& id : mMoverPanel->effectiveTargets()) add_actor(id);",
        "for (const LLUUID& id : mShell->castSelection()) add_actor(id);"),
    "look_pages_untargeted": ("director",
        "ALDirectorTagTargets::appendSubjectTargets(kind, mTagSelectionScratch);", "(void)kind;"),
    "self_not_normalised": ("director", "ALDirectorTagTargets::canonicalId(id)", "id"),
    "resolver_labels_skip_self": ("resolver",
        "const LLUUID id = ALDirectorTagTargets::canonicalId(member.mId);", "const LLUUID id = member.mId;"),
    "resolver_ignores_group_revision": ("resolver",
        "mGroups.refresh(cast.getCastRevision(), cast.getGroupRevision()",
        "mGroups.refresh(cast.getCastRevision(), 0"),
    "resolver_keeps_labels_without_cast": ("resolver", "mGroups.invalidate();", "(void)0;"),
    "resolver_ignores_toggle": ("resolver", SINGLE_LOOKUP, SINGLE_LOOKUP.replace(" highlightEnabled() &&", "")),
    "resolver_ignores_session": ("resolver", SINGLE_LOOKUP, SINGLE_LOOKUP.replace(" sessionAllowsHighlight() &&", "")),
    "resolver_no_heartbeat": ("resolver", DOUBLE_LOOKUP,
        L("    return !mIds.empty() && highlightEnabled() && sessionAllowsHighlight() &&",
          "        (mIds.contains(first) || mIds.contains(second));")),
    "resolver_anyone_withdraws": ("resolver",
        L("    if (owner != mOwner)", "    {"), L("    if (false)", "    {")),
    "resolver_publishes_while_teleporting": ("resolver",
        L("    if (!sessionAllowsHighlight())", "    {", "        withdraw(owner);"),
        L("    if (false)", "    {", "        withdraw(owner);")),
    "rig_object_target_keeps_snapshot": ("rig",
        L("        // An object target lights no avatar: a failed resolution must not keep an earlier snapshot.",
          "        mLastResolvedMembers.clear();"),
        "        // snapshot kept"),
    "round3_roto_gather_not_recorded": ("pipeline",
        "tag_view ? &mRotoInkTagRecord.mIds : nullptr);", "nullptr);"),
    "round3_roto_subject_not_recorded": ("pipeline",
        "mRotoInkTagRecord.mIds.push_back(avatar->getID());", "(void)avatar;"),
    "round3_tron_gather_not_recorded": ("pipeline",
        "tag_view && mask_consumed ? &mTronSubjectTagRecord.mIds : nullptr);", "nullptr);"),
    "round3_gather_selection_not_reported": ("pipeline",
        "            out_selected_ids->push_back(c.mId);", "            (void)c;"),
    "round3_trail_selection_not_recorded": ("tron",
        "sTrailTagIds.push_back(cand.mId);", "(void)cand;"),
    "round3_roto_stale_record_shown": ("resolver",
        "if (ALTagHighlight::recordFresh(record.mRecorded, LLFrameTimer::getFrameCount(), record.mFrame))",
        "if (true)"),
    "round3_tron_stale_mask_shown": ("resolver",
        "if (ALTagHighlight::recordFresh(mask.mRecorded, frame, mask.mFrame))", "if (true)"),
    "round3_tron_stale_trails_shown": ("resolver",
        "if (trails && ALTagHighlight::recordFresh(trail_recorded, frame, trail_frame))", "if (trails)"),
    "round3_roto_record_discarded": ("resolver",
        "out.insert(out.end(), record.mIds.begin(), record.mIds.end());", "(void)record;"),
    "round3_tron_record_discarded": ("resolver",
        "out.insert(out.end(), mask.mIds.begin(), mask.mIds.end());", "(void)mask;"),
    "round3_ui_gather_added": ("resolver",
        "// What the Roto Ink render pass really isolated (subject or capped target set).",
        "gPipeline.gatherRotoInkTargets(0, 1, 1.f, 1.f, 0, gGLLastModelView, gGLLastProjection, nullptr, nullptr);"),
    "round3_shadows_refresh_names": ("resolver", "cast.resolve(subject, false)", "cast.resolve(subject)"),
    "round3_renderer_has_selection_only_mode": ("pipeline",
        "        if (out_selected_ids)", "        bool selection_only = true; (void)selection_only;\n        if (out_selected_ids)"),
    "round4_gather_records_before_projection": ("pipeline",
        L("    const S32 cap = std::clamp(max_targets, 1, 16);", "    if ((S32)culled.size() > cap)", "    {",
          "        culled.resize(cap);", "    }"),
        L("    const S32 cap = std::clamp(max_targets, 1, 16);", "    if ((S32)culled.size() > cap)", "    {",
          "        culled.resize(cap);", "    }",
          "    if (out_selected_ids)", "    {", "        for (const Culled& c : culled)", "        {",
          "            out_selected_ids->push_back(c.mId);", "        }", "    }")),
    "round4_roto_records_any_view": ("pipeline",
        L("", "    const bool tag_view = isTagRecordView();"), L("", "    const bool tag_view = true;")),
    "round4_tron_records_any_view": ("pipeline",
        L("", "        const bool tag_view = isTagRecordView();"), L("", "        const bool tag_view = true;")),
    "round5_sensor_overlay_not_recorded": ("pipeline",
        L("", "    const bool tag_view = isTagRecordView();"), L("", "    const bool tag_view = isTagRecordView() && !sensor_overlay;")),
    "round5_preview_view_records": ("pipeline",
        " && !sPrismLensRender && !sPrismPreviewRender;", " && !sPrismLensRender;"),
    "round5_paused_trail_ids_not_rebuilt": ("tron",
        L("                sHasGeometry = true;", "                sTrailTagIds.push_back(kv.first);"),
        L("                sHasGeometry = true;", "                (void)kv;")),
    "round7_trail_record_before_eviction": ("tron",
        "    // --- 5b: eviction BEFORE insertion (P1-1) -------------------------------",
        L("    sTrailTagIds.clear();", "    for (const auto& cand : cands)", "    {", "        sTrailTagIds.push_back(cand.mId);", "    }",
          "    // --- 5b: eviction BEFORE insertion (P1-1) -------------------------------")),
    "round6_running_trails_drop_drawn_histories": ("tron",
        L("        if (trailDrawable(kv.second))", "        {", "            sTrailTagIds.push_back(kv.first);"),
        L("        if (trailDrawable(kv.second))", "        {", "            (void)kv;")),
    "round6_stale_override_draws": ("nametag",
        "ALDirectorTagTargets::instance().overrideActive(mBackgroundOverrideStamp);", "true;"),
    "round6_override_ignores_style_stamp": ("resolver",
        "return stamp == mStyleStamp && !mIds.empty()", "return !mIds.empty()"),
    "round5_tron_grade_keep_without_grade": ("pipeline",
        "(grade_strength > 0.f && grade_keep_subject > 0.f) ||", "(grade_keep_subject > 0.f) ||"),
    "round5_tron_exclude_without_surface": ("pipeline",
        "(surface_consumer && grid_subject_exclude > 0.f) || rim_on;", "(grid_subject_exclude > 0.f) || rim_on;"),
    "round5_tron_anchors_without_surface": ("pipeline",
        "const bool anchors_consumed = surface_consumer && grid_subject_radius > 0.f;",
        "const bool anchors_consumed = grid_subject_radius > 0.f;"),
    "round5_roto_subject_without_valid_extents": ("pipeline",
        L("            if (raw_center.isFinite() && raw_half.isFinite())", "            {", "                if (tag_view && (subject_mode == 1"),
        L("            if (true)", "            {", "                if (tag_view && (subject_mode == 1")),
    "round4_tron_anchors_not_recorded": ("pipeline",
        "                mTronSubjectTagRecord.mIds.push_back(candidates[i].mId);", "                (void)candidates[i];"),
    "round4_tron_mask_recorded_when_unconsumed": ("pipeline",
        "tag_view && mask_consumed ? &mTronSubjectTagRecord.mIds : nullptr);", "tag_view ? &mTronSubjectTagRecord.mIds : nullptr);"),
    "round4_paused_trails_expire": ("tron",
        L("            sTrailTagFrame = LLFrameTimer::getFrameCount();", "        }", "        return; // no emission"),
        L("            (void)sTrailTagFrame;", "        }", "        return; // no emission")),
    "round4_hidden_director_keeps_highlight": ("director",
        L("    if (!visible)", "        ALDirectorTagTargets::instance().withdraw(this);"),
        L("    if (!visible)", "        (void)visible;")),
    "round4_off_rig_still_published": ("director",
        "if (rigs.isSlotLit(rigs.selectedSlot()))", "if (true)"),
    "round4_ui_hidden_still_highlights": ("resolver",
        "gViewerWindow && gViewerWindow->getUIVisibility() && !gAgentCamera.cameraMouselook();", "true;"),
    "round4_crowd_counts_unanchored_member": ("studio",
        L("            if (!nameplateAnchor(*inst, member_anchor))", "            {", "                continue;", "            }",
          "            // Only a member that really contributes to the crowd plate counts for its highlight"),
        L("            if (!nameplateAnchor(*inst, member_anchor))", "            {", "            }",
          "            // Only a member that really contributes to the crowd plate counts for its highlight")),
    "round2_look_targets_skip_add_actor": ("director",
        "        for (const LLUUID& id : mTagSelectionScratch) add_actor(id);\n        break;\n    case ALTagHighlight::PageTarget::NONE:",
        "        ids.insert(ids.end(), mTagSelectionScratch.begin(), mTagSelectionScratch.end());\n        break;\n    case ALTagHighlight::PageTarget::NONE:"),
    "round2_heartbeat_only_at_10hz": ("director", "    targets.touch(this);\n", ""),
    "round2_destructor_leaves_highlight": ("director",
        "    ALDirectorTagTargets::instance().withdraw(this);\n    if (LLContextMenu* menu = mCameraSubjectMenu.get()) menu->die();",
        "    if (LLContextMenu* menu = mCameraSubjectMenu.get()) menu->die();"),
    "round2_owner_not_released": ("resolver", "    mOwner = nullptr;\n", ""),
    "round2_owner_not_sticky": ("resolver", "    if (mOwner && mOwner != owner &&", "    if (false &&"),
    "round2_null_not_you": ("resolver",
        L("    if (id.isNull())", "    {", "        return gAgentID;"),
        L("    if (false)", "    {", "        return gAgentID;")),
    "round2_labels_miss_clone_identity": ("resolver",
        "add(clone->mEntityId, member.mGroup);", "(void)clone;"),
    "round2_empty_group_selects_ungrouped": ("director",
        L("        const std::string group = selectedManagedGroup();", "        if (!group.empty())"),
        L("        const std::string group = selectedManagedGroup();", "        if (true)")),
    "round2_highlight_ignores_rlv": ("avatar",
        "(isSelf() || RlvActions::canShowName(RlvActions::SNC_DEFAULT, getID()));", "true;"),
    "round2_label_strip_not_highlighted": ("nametag",
        "label_top_color = mBackgroundOverride;", "(void)label_top_color;"),
    "round2_legacy_snapshot_not_filled": ("rig",
        "            if (group_members[i]) mLastResolvedMembers.push_back(group_members[i]->getID());",
        "            (void)group_members[i];"),
    "round2_move_everyone_returns_empty": ("mover", "        return everyone;", "        return uuid_vec_t();"),
    "round2_move_selection_returns_empty": ("mover",
        L("    return selectedTargets();", "}", "", "void ALPanelActorMover::onClickWalk()"),
        L("    return uuid_vec_t();", "}", "", "void ALPanelActorMover::onClickWalk()")),
    "rig_legacy_group_not_published": ("rig",
        L("    if (target == LitTarget::LEGACY)", "    {",
          "        // Legacy slot groups never filled the identity snapshot; publish who this rig really lights.",
          "        // The named-group fields stay consistent: no stored identities, none counted as present."),
        L("    if (false)", "    {", "        // legacy skipped")),
    "round3_legacy_stored_not_cleared": ("rig",
        L("        mLastResolvedStoredMembers.clear();", "        mLitGroupPresent = 0;",
          "        for (S32 i = 0; i < group_count; ++i)"),
        L("        mLitGroupPresent = 0;", "        for (S32 i = 0; i < group_count; ++i)")),
}


def run_wiring():
    sources = wiring_sources()
    errors = wiring_errors(sources)
    if errors:
        raise RuntimeError("wiring: " + "; ".join(errors))
    print("PASS: wiring (name tag, clone plates, Director, resolver, rig, setting)", flush=True)
    for name, (key, before, after) in WIRING_NEGATIVES.items():
        assert sources[key].count(before) == 1, "planted wiring edit does not apply exactly once: " + name
        mutated = dict(sources)
        mutated[key] = sources[key].replace(before, after)
        if not wiring_errors(mutated):
            raise RuntimeError("planted wiring negative unexpectedly passed: " + name)
        print("PASS: rejected planted wiring " + name, flush=True)


def main(argv):
    if "--selftest" in argv:
        run_wiring()
        return 0
    accents = theme_accents()
    directory = check_directory("director_tag_highlight")
    flags, libraries = viewer_native_flags()
    flags += ["/UNDEBUG"]    # assertions and checks must run with Release dependency flags
    header = read(HEADER)
    result = run_fixture(directory, flags, libraries, header, "tag_highlight", accents)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    print(result.stdout.strip(), flush=True)

    for name, (before, after) in HEADER_NEGATIVES.items():
        assert header.count(before) == 1, "planted edit does not apply exactly once: " + name
        outcome = run_fixture(directory, flags, libraries, header.replace(before, after), "neg_" + name, accents)
        if outcome.returncode == 0:
            raise RuntimeError("planted negative unexpectedly passed: " + name)
        print("PASS: rejected planted " + name, flush=True)
    run_wiring()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except RuntimeError as error:
        print(error, file=sys.stderr)
        sys.exit(1)
