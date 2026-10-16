/**
 * @file alsettingsundohistorymodel_test.cpp
 * @brief Tests for the pure settings undo/redo history model.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alsettingsundohistorymodel.h"

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
}
