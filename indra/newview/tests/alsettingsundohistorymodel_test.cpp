/**
 * @file alsettingsundohistorymodel_test.cpp
 * @brief Tests for the pure settings undo/redo history model.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alsettingsundohistorymodel.h"
#include "../alundosequence.h"
#include "../alundovalue.h"
#include "../alkithistory.h"

namespace tut
{
struct alsettingsundohistory_data {};
typedef test_group<alsettingsundohistory_data> alsettingsundohistory_group;
typedef alsettingsundohistory_group::object alsettingsundohistory_object;
alsettingsundohistory_group gALSettingsUndoHistory("alsettingsundohistory");

template<> template<>
void alsettingsundohistory_object::test<1>()
{
    ALSettingsUndoHistoryModel model;
    model.record("Sun", "Sun EV", 0.0, 0.1, 1000);
    model.record("Sun", "Sun EV", 0.1, 0.2, 1200);
    model.record("Sun", "Sun EV", 0.2, 0.3, 1499);

    ALSettingsUndoHistoryModel::Step step;
    ensure("coalesced drag is undoable", model.takeUndo(step));
    ensure_equals("one coalesced change", step.mChanges.size(), std::size_t(1));
    ensure_equals("keeps first value", step.mChanges[0].mOldValue.asReal(), 0.0);
    ensure_equals("keeps last value", step.mChanges[0].mNewValue.asReal(), 0.3);
    ensure_equals("step label", step.mLabel, std::string("Sun EV"));
}

template<> template<>
void alsettingsundohistory_object::test<2>()
{
    ALSettingsUndoHistoryModel model;
    model.record("Sun", "Sun EV", 0.0, 1.0, 1000);
    model.closeIdle(1500);
    model.record("Sun", "Sun EV", 1.0, 2.0, 1501);

    ALSettingsUndoHistoryModel::Step step;
    ensure("second edit undo", model.takeUndo(step));
    ensure_equals("second edit begins at one", step.mChanges[0].mOldValue.asReal(), 1.0);
    ensure("first edit remains", model.takeUndo(step));
    ensure_equals("first edit begins at zero", step.mChanges[0].mOldValue.asReal(), 0.0);
}

template<> template<>
void alsettingsundohistory_object::test<3>()
{
    ALSettingsUndoHistoryModel model;
    model.beginBatch("Preset 'Golden Hour'");
    model.record("Sun", "Sun EV", 0.0, 0.5, 1000);
    model.record("Sky", "Sky EV", 0.0, -0.5, 1001);
    model.record("Sun", "Sun EV", 0.5, 0.75, 1002);
    model.endBatch();

    ALSettingsUndoHistoryModel::Step step;
    ensure("batch undo", model.takeUndo(step));
    ensure_equals("two keys in batch", step.mChanges.size(), std::size_t(2));
    ensure_equals("named batch", step.mLabel, std::string("Preset 'Golden Hour'"));
    ensure_equals("same key merged", step.mChanges[0].mNewValue.asReal(), 0.75);
}

template<> template<>
void alsettingsundohistory_object::test<4>()
{
    ALSettingsUndoHistoryModel model;
    model.record("Sun", "Sun EV", 0.0, 1.0, 1000);
    ALSettingsUndoHistoryModel::Step step;
    ensure("undo available", model.takeUndo(step));
    ensure("redo available", model.canRedo());
    ensure("redo taken", model.takeRedo(step));
    ensure("undo restored after redo", model.canUndo());
    ensure("redo consumed", !model.canRedo());

    ensure("undo again", model.takeUndo(step));
    model.record("Sky", "Sky EV", 0.0, 1.0, 2000);
    ensure("new edit clears redo", !model.canRedo());
}

template<> template<>
void alsettingsundohistory_object::test<5>()
{
    ALSettingsUndoHistoryModel model;
    model.record("Sun", "Sun EV", 1.0, 1.0, 1000);
    ensure("no-op ignored", !model.canUndo());

    LLSD color_a = LLSD::emptyArray();
    color_a.append(1.0); color_a.append(0.5); color_a.append(0.25); color_a.append(1.0);
    LLSD color_b = LLSD::emptyArray();
    color_b.append(1.0); color_b.append(0.5); color_b.append(0.25); color_b.append(1.0);
    model.record("Tint", "Sun tint", color_a, color_b, 1000);
    ensure("deep LLSD no-op ignored", !model.canUndo());

    model.beginBatch("Reset All");
    model.record("Sun", "Sun EV", 0.0, 1.0, 1001);
    model.record("Sun", "Sun EV", 1.0, 0.0, 1002);
    model.endBatch();
    ensure("batch net no-op ignored", !model.canUndo());
}

template<> template<>
void alsettingsundohistory_object::test<6>()
{
    ALSettingsUndoHistoryModel model;
    for (U64 i = 0; i < ALSettingsUndoHistoryModel::MAX_DEPTH + 5; ++i)
    {
        model.record("Key" + std::to_string(i), "Edit", LLSD(0), LLSD(1), i * 1000);
    }

    ALSettingsUndoHistoryModel::Step step;
    std::size_t count = 0;
    while (model.takeUndo(step))
    {
        ++count;
    }
    ensure_equals("history is capped", count, ALSettingsUndoHistoryModel::MAX_DEPTH);
}

template<> template<>
void alsettingsundohistory_object::test<7>()
{
    ALSettingsUndoHistoryModel model;
    model.beginEdit();
    model.record("Sun", "Sun EV", 0.0, 0.5, 1000);
    model.closeIdle(2000);
    model.record("Sun", "Sun EV", 0.5, 1.0, 2000);
    model.endEdit();

    ALSettingsUndoHistoryModel::Step step;
    ensure("slow drag is one undo", model.takeUndo(step));
    ensure_equals("slow drag has one change", step.mChanges.size(), std::size_t(1));
    ensure_equals("slow drag keeps first value", step.mChanges[0].mOldValue.asReal(), 0.0);
    ensure_equals("slow drag keeps final value", step.mChanges[0].mNewValue.asReal(), 1.0);
    ensure("slow drag leaves no earlier step", !model.takeUndo(step));
}

template<> template<>
void alsettingsundohistory_object::test<8>()
{
    ALSettingsUndoHistoryModel model;
    model.record("Sun", "Sun EV", 0.0, 1.0, 1000);
    model.endEdit();

    ALSettingsUndoHistoryModel::Step step;
    ensure("create redo branch", model.takeUndo(step));
    model.record("Sky", "Sky EV", 0.0, 1.0, 2000);
    model.record("Sky", "Sky EV", 1.0, 0.0, 2100);
    ensure("net no-op preserves redo", model.canRedo());

    model.clear();
    ensure("clear removes undo", !model.canUndo());
    ensure("clear removes redo", !model.canRedo());
}

template<> template<>
void alsettingsundohistory_object::test<9>()
{
    ALSettingsUndoHistoryModel exposure;
    ALSettingsUndoHistoryModel image;
    exposure.record("RenderExposure", "Exposure", 1.0, 2.0, 1000);
    const U64 first = exposure.stepSequence(false);
    image.record("RenderExposure", "Exposure", 2.0, 3.0, 1001);
    ensure("independent page models have a common ordering", image.stepSequence(false) > first);
    ensure_equals("another page cannot coalesce into this page", exposure.stepSequence(false), first);

    ALSettingsUndoHistoryModel::Step step;
    ensure("exposure owns its pending step", exposure.takeUndo(step));
    ensure_equals("exposure keeps its own new value", step.mChanges[0].mNewValue.asReal(), 2.0);
    const U64 exposure_redo = exposure.stepSequence(true);
    ensure("image owns its step", image.takeUndo(step));
    ensure("redo order follows undo order", image.stepSequence(true) > exposure_redo);
    ensure_equals("undoing image leaves exposure redo intact", exposure.stepSequence(true), exposure_redo);
}

template<> template<>
void alsettingsundohistory_object::test<10>()
{
    ALSettingsUndoHistoryModel model;
    model.beginEdit();
    model.record("RenderExposure", "Exposure", 1.0, 2.0, 1000);
    const U64 first = model.stepSequence(false);
    model.record("RenderExposure", "Exposure", 2.0, 3.0, 3000);
    const U64 last = model.stepSequence(false);
    ensure("open drags publish their latest change", last > first);
    model.endEdit();
    ensure_equals("finishing a gesture preserves chronology", model.stepSequence(false), last);
    ALSettingsUndoHistoryModel::Step step;
    ensure("one slow drag undoes", model.takeUndo(step));
    ensure_equals("slow drag restores its first value", step.mChanges[0].mOldValue.asReal(), 1.0);
    ensure_equals("slow drag replays its final value", step.mChanges[0].mNewValue.asReal(), 3.0);
    ensure("slow drag remains one step", !model.canUndo());
}

template<> template<>
void alsettingsundohistory_object::test<11>()
{
    ALSettingsUndoHistoryModel model;
    model.record("RenderExposure", "Exposure", 1.0, 2.0, 1000);
    ALSettingsUndoHistoryModel::Step step;
    auto conflict = [](const ALSettingsUndoHistoryModel::Change&) { return false; };
    ensure("conflicting undo consumes the step", model.takeUndo(step, conflict));
    ensure("conflict is omitted from replay", step.mChanges.empty());
    ensure("conflict is gone from undo", !model.canUndo());
    ensure("conflict is gone from redo", !model.canRedo());
    ensure("redo cannot overwrite the other page", !model.takeRedo(step));

    model.beginBatch("Mixed");
    model.record("Conflict", "Conflict", 1.0, 2.0, 2000);
    model.record("Safe", "Safe", 3.0, 4.0, 2001);
    model.endBatch();
    ensure("partial undo", model.takeUndo(step,
        [](const ALSettingsUndoHistoryModel::Change& change) { return change.mKey != "Conflict"; }));
    ensure_equals("only safe entry survives", step.mChanges.size(), std::size_t(1));
    ensure("partial redo", model.takeRedo(step));
    ensure_equals("redo contains only safe entry", step.mChanges[0].mKey, std::string("Safe"));
    ensure("conflicting redo is also consumed", model.takeUndo(step));
    ensure("skip redo", model.takeRedo(step, conflict));
    ensure("skipped redo leaves neither branch", !model.canUndo() && !model.canRedo());
}

template<> template<>
void alsettingsundohistory_object::test<12>()
{
    ensure("float ignores LLSD numeric representation", alUndoValuesEqual(TYPE_F32, LLSD(2), LLSD(2.0)));
    ensure("float tolerates F32 rounding", alUndoValuesEqual(TYPE_F32, LLSD(0.1), LLSD(F64(F32(0.1)))));
    ensure("float uses relative epsilon", alUndoValuesEqual(TYPE_F32, LLSD(10000.0), LLSD(10000.05)));
    ensure("float detects real changes", !alUndoValuesEqual(TYPE_F32, LLSD(1.0), LLSD(1.001)));
    ensure("tiny settings retain their small steps", !alUndoValuesEqual(TYPE_F32, LLSD(0.000001), LLSD(0.000002)));
    ensure("near zero rounding stays bounded", alUndoValuesEqual(TYPE_F32, LLSD(0.0), LLSD(0.0000000005)));
    ensure("integers accept equivalent representation", alUndoValuesEqual(TYPE_S32, LLSD(2), LLSD(2.0)));
    ensure("integers compare exactly", !alUndoValuesEqual(TYPE_S32, LLSD(2), LLSD(2.000001)));
    ensure("unsigned integers compare exactly", !alUndoValuesEqual(TYPE_U32, LLSD(10), LLSD(11)));
    ensure("booleans compare exactly", !alUndoValuesEqual(TYPE_BOOLEAN, LLSD(true), LLSD(false)));
    ensure("strings compare exactly", !alUndoValuesEqual(TYPE_STRING, LLSD("1"), LLSD("1.0")));
    LLSD a = LLSD::emptyArray(), b = LLSD::emptyArray();
    a.append(1); a.append(0.1); a.append(0.5);
    b.append(1.0); b.append(F64(F32(0.1))); b.append(0.5);
    ensure("colours compare each component", alUndoValuesEqual(TYPE_COL3, a, b));
    ensure("vectors compare each component", alUndoValuesEqual(TYPE_VEC3, a, b));
    b[2] = 0.51;
    ensure("component conflict is preserved", !alUndoValuesEqual(TYPE_COL3, a, b));
    b.append(1.0);
    ensure("component counts must match", !alUndoValuesEqual(TYPE_VEC3, a, b));
}

template<> template<>
void alsettingsundohistory_object::test<13>()
{
    ALSettingsUndoHistoryModel exposure, kit;
    U64 action = 0;
    {
        ALScopedUndoSequence group;
        action = alNextUndoSequence();
        exposure.beginBatch("Reset page");
        kit.beginBatch("Reset page");
        exposure.record("RenderExposure", "Exposure", 1.0, 2.0, 1000);
        kit.record("Tonemap", "Tonemap", 0, 1, 1001);
        {
            ALScopedUndoSequence nested;
            exposure.record("Sun", "Sun", 0.0, 1.0, 1002);
            ensure_equals("nested actions retain the group", alNextUndoSequence(), action);
        }
        exposure.endBatch();
        kit.endBatch();
    }
    ensure_equals("owned portion uses action sequence", exposure.stepSequence(false), action);
    ensure_equals("kit portion uses action sequence", kit.stepSequence(false), action);
    ensure("next independent action is newer", alNextUndoSequence() > action);
    ALSettingsUndoHistoryModel::Step step;
    {
        ALScopedUndoSequence replay;
        ensure("undo owned portion", exposure.takeUndo(step));
        ensure("undo kit portion", kit.takeUndo(step));
    }
    ensure_equals("redo retains one shared group", exposure.stepSequence(true), kit.stepSequence(true));
    {
        ALScopedUndoSequence replay;
        ensure("redo kit portion", kit.takeRedo(step));
        ensure("redo owned portion", exposure.takeRedo(step));
    }
    ensure_equals("repeated undo retains one shared group", exposure.stepSequence(false), kit.stepSequence(false));
    ensure("compound action leaves no earlier owned step", exposure.takeUndo(step) && !exposure.canUndo());
    ensure("compound action leaves no earlier kit step", kit.takeUndo(step) && !kit.canUndo());
}

template<> template<>
void alsettingsundohistory_object::test<14>()
{
    ALSettingsUndoHistoryModel rig;
    rig.record("Kelvin", "Key temperature", 3000.0, 4000.0, 1000);
    const LLSD quick_value = 5000.0;
    ALSettingsUndoHistoryModel::Step step;
    ensure("widget conflict consumes the entry", rig.takeUndo(step, [&quick_value](const auto& change)
        { return alUndoWidgetValuesEqual(quick_value, change.mNewValue); }));
    ensure("widget conflict never reaches redo", step.mChanges.empty() && !rig.canUndo() && !rig.canRedo());
    ensure("widget numeric representation compares by type", alUndoWidgetValuesEqual(LLSD(4000), LLSD(4000.0)));
    ensure("widget strings stay exact", !alUndoWidgetValuesEqual(LLSD("4000"), LLSD("4000.0")));
    ensure("widget boolean conflicts", !alUndoWidgetValuesEqual(LLSD(false), LLSD(true)));
}

template<> template<>
void alsettingsundohistory_object::test<15>()
{
    ALKitHistory::Step compound;
    ALKitHistory::Entry rig, image;
    rig.binding_page = "light_group/cine_light_rig_tab";
    rig.context = "Self rig, Key fixture, Key light";
    image.binding_page = "look_group/image_tab";
    compound.entries = {rig, image};
    ensure("unrelated entry has no rig guard", compound.entries[1].contextMatches(""));
    ensure("later image entry cannot erase rig guard", !compound.entries[0].contextMatches("Self rig, Fill fixture, Key light"));
    ensure("original rig context remains replayable", compound.entries[0].contextMatches(rig.context));
}

template<> template<>
void alsettingsundohistory_object::test<16>()
{
    ALSettingsUndoHistoryModel exposure, kit;
    {
        ALScopedUndoSequence group;
        exposure.record("Exposure", "Exposure", 1.0, 2.0, 1000);
        kit.record("Widget", "Widget", 3.0, 4.0, 1001);
    }
    const U64 sequence = exposure.stepSequence(false);
    ensure_equals("both parts belong to one action", kit.stepSequence(false), sequence);
    const auto safe = [](const auto&) { return true; };
    const auto blocked = [](const auto&) { return false; };
    for (bool redo : {false, true})
    {
        if (redo)
        {
            ALSettingsUndoHistoryModel::Step step;
            ALScopedUndoSequence group;
            ensure("prepare redo exposure", exposure.takeUndo(step));
            ensure("prepare redo kit", kit.takeUndo(step));
        }
        const U64 before = exposure.stepSequence(redo);
        ensure("group preflight rejects one blocked part",
            !(exposure.replayReady(redo, safe) && kit.replayReady(redo, blocked)));
        ensure_equals("preflight leaves earlier owner unapplied", exposure.stepSequence(redo), before);
        ensure("valid group passes", exposure.replayReady(redo, safe) && kit.replayReady(redo, safe));
    }
    exposure.discardStep(true);
    kit.discardStep(true);
    ensure("skipped group is absent from both stacks", !exposure.canUndo() && !exposure.canRedo() && !kit.canUndo() && !kit.canRedo());
}
}
