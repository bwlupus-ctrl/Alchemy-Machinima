/**
 * @file alsettingsundohistory.cpp
 * @brief Session-only undo/redo for the shared exposure settings.
 */

#include "llviewerprecompiledheaders.h"

#include "alsettingsundohistory.h"
#include "alkithistory.h"
#include "alkitpage.h"
#include "alundovalue.h"

#include "llbutton.h"
#include "llcontrol.h"
#include "lleventtimer.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltimer.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>

namespace
{
const ALExposureSettingsHistory::TrackedSetting TRACKED_SETTINGS[] =
{
    { "AlchemyEnvSunEV",               "Sun EV" },
    { "AlchemyEnvSkyGIEV",             "Sky light (GI) EV" },
    { "AlchemyEnvMoonLinked",          "Link Moon to Sun" },
    { "AlchemyEnvMoonEV",              "Moon EV" },
    { "AlchemyEnvGIAmbientEV",         "Sky ambient EV" },
    { "AlchemyEnvGIProbeDiffuseEV",    "Probe diffuse EV" },
    { "AlchemyEnvGIProbeSpecEV",       "Probe reflections EV" },
    { "AlchemyEnvLocalLightEV",        "Local lights EV" },
    { "AlchemyEnvLocalLightIncludeRig", "Include rig lights" },
    { "AlchemyEnvSunKelvin",           "Sun temperature" },
    { "AlchemyEnvSunTintColor",        "Sun tint" },
    { "AlchemyEnvSunTintStrength",     "Sun tint strength" },
    { "AlchemyEnvMoonTintColor",       "Moon tint" },
    { "AlchemyEnvMoonTintStrength",    "Moon tint strength" },
    { "AlchemyEnvAmbientTintColor",    "Ambient tint" },
    { "AlchemyEnvAmbientTintStrength", "Ambient tint strength" },
    { "AlchemyEnvShadowLiftEV",        "Shadow lift EV" },
    { "RenderDynamicExposureEnabled",  "Auto exposure" },
    { "RenderExposure",                "Camera exposure" },
};

U64 nowMilliseconds()
{
    return static_cast<U64>(static_cast<F64>(LLTimer::getTotalSeconds()) * 1000.0);
}

class HistoryService final : public LLEventTimer
{
public:
    HistoryService()
    :   LLEventTimer(0.25f)
    {
        for (const auto& setting : TRACKED_SETTINGS)
        {
            if (LLControlVariable* control = gSavedSettings.getControl(setting.mKey))
            {
                mConnections.emplace_back(control->getSignal()->connect(
                    [this, key = std::string(setting.mKey), label = std::string(setting.mLabel)](
                        LLControlVariable*, const LLSD& new_value, const LLSD& old_value)
                    {
                        if (!mApplying && !ALKitHistory::suspended())
                        {
                            if (ALKitPage* page = dynamic_cast<ALKitPage*>(ALKitHistory::inputPage()))
                                page->historyEdited(true);
                            currentModel().record(key, label, old_value, new_value, nowMilliseconds());
                            publishRevision();
                        }
                    }));
            }
            else
            {
                LL_WARNS("ExposureHistory") << "Missing tracked setting "
                                             << setting.mKey << LL_ENDL;
            }
        }
    }

    bool tick() override
    {
        const U64 now = nowMilliseconds();
        auto close = [now](ALSettingsUndoHistoryModel& model)
        {
            if (model.isEditActive() && !gFocusMgr.getMouseCapture())
                model.endEdit();
            model.closeIdle(now);
        };
        close(mModel);
        for (auto it = mPageModels.begin(); it != mPageModels.end();)
        {
            if (it->first.isDead() &&
                std::find(mBatches.begin(), mBatches.end(), &it->second.model) == mBatches.end())
            {
                it = mPageModels.erase(it);
                mRevisionDirty = true;
            }
            else
            {
                close(it->second.model);
                ++it;
            }
        }
        publishRevision();
        return false;
    }

    void beginBatch(const std::string& label)
    {
        ALSettingsUndoHistoryModel* model = &currentModel();
        mBatches.push_back(model);
        model->beginBatch(label);
    }

    void endBatch()
    {
        if (mBatches.empty())
            return;
        ALSettingsUndoHistoryModel* model = mBatches.back();
        mBatches.pop_back();
        const U64 before = model->revision();
        model->endBatch();
        if (model->revision() != before)
        {
            publishRevision();
        }
    }

    void endEdit()
    {
        mModel.endEdit();
        for (auto& item : mPageModels)
            item.second.model.endEdit();
        publishRevision();
    }

    void beginEdit()
    {
        currentModel().beginEdit();
        publishRevision();
    }

    void clear()
    {
        mModel.clear();
        for (auto& item : mPageModels)
            item.second.model.clear();
        publishRevision();
    }

    void undo()
    {
        replayModel(mModel, false);
    }

    void redo()
    {
        replayModel(mModel, true);
    }

    ALSettingsUndoHistoryModel& model() { return mModel; }
    ALSettingsUndoHistoryModel& pageModel(LLView* page)
    {
        if (!page)
            return mModel;
        return mPageModels[page->getHandle()].model;
    }
    void replayPage(LLView* page, bool redo)
    {
        replayModel(pageModel(page), redo);
    }
    bool pageReplayReady(LLView* page, bool redo)
    {
        return pageModel(page).replayReady(redo, [redo](const ALSettingsUndoHistoryModel::Change& change)
        {
            LLControlVariable* control = gSavedSettings.getControl(change.mKey).get();
            return control && alUndoValuesEqual(control->type(), control->get(),
                redo ? change.mOldValue : change.mNewValue);
        });
    }
    void discardPageStep(LLView* page, bool redo) { pageModel(page).discardStep(redo); publishRevision(); }
    U64 revision() const { return mRevision; }
    void finishPage(LLView* page) { pageModel(page).endEdit(); publishRevision(); }
    void discardPageRedo(LLView* page) { pageModel(page).discardRedo(); publishRevision(); }
    ALExposureSettingsHistory::revision_signal_t& signal() { return mRevisionSignal; }

private:
    ALSettingsUndoHistoryModel& currentModel()
    {
        return mBatches.empty() ? pageModel(ALKitHistory::inputPage()) : *mBatches.back();
    }
    void replayModel(ALSettingsUndoHistoryModel& model, bool use_new_value)
    {
        ALKitHistory::ScopedReplay replay;
        mApplying = true;
        bool skipped = false;
        auto filter = [&skipped, use_new_value](const ALSettingsUndoHistoryModel::Change& change)
        {
            if (LLControlVariable* control = gSavedSettings.getControl(change.mKey))
            {
                if (alUndoValuesEqual(control->type(), control->get(),
                    use_new_value ? change.mOldValue : change.mNewValue))
                    return true;
            }
            skipped = true;
            return false;
        };
        ALSettingsUndoHistoryModel::Step step;
        // Filter before transferring the step: conflicts must never survive on the other stack.
        if (!(use_new_value ? model.takeRedo(step, filter) : model.takeUndo(step, filter)))
        {
            mApplying = false;
            return;
        }
        auto apply_change = [use_new_value](const ALSettingsUndoHistoryModel::Change& change)
        {
            if (LLControlVariable* control = gSavedSettings.getControl(change.mKey))
                control->setValue(use_new_value ? change.mNewValue : change.mOldValue);
        };
        if (use_new_value)
        {
            for (const auto& change : step.mChanges)
            {
                apply_change(change);
            }
        }
        else
        {
            for (auto change = step.mChanges.rbegin(); change != step.mChanges.rend(); ++change)
            {
                apply_change(*change);
            }
        }
        mApplying = false;
        if (skipped)
            LLNotificationsUtil::add("SystemMessageTip",
                LLSD().with("MESSAGE", "Skipped settings changed elsewhere"));
        publishRevision(true);
    }

    void publishRevision(bool force = false)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ExposureHistory publishRevision");
        bool changed = mRevisionDirty || mPublishedLegacyRevision != mModel.revision();
        mPublishedLegacyRevision = mModel.revision();
        for (auto& item : mPageModels)
        {
            changed = changed || item.second.publishedRevision != item.second.model.revision();
            item.second.publishedRevision = item.second.model.revision();
        }
        mRevisionDirty = false;
        if (changed || force)
        {
            ++mRevision;
            mRevisionSignal(mRevision);
        }
    }

    ALSettingsUndoHistoryModel mModel;
    struct PageModel
    {
        ALSettingsUndoHistoryModel model;
        U64 publishedRevision = 0;
    };
    std::map<LLHandle<LLView>, PageModel> mPageModels;
    std::vector<ALSettingsUndoHistoryModel*> mBatches;
    bool mApplying = false;
    bool mRevisionDirty = false;
    U64 mPublishedLegacyRevision = 0;
    U64 mRevision = 1;
    std::vector<boost::signals2::scoped_connection> mConnections;
    ALExposureSettingsHistory::revision_signal_t mRevisionSignal;
};

HistoryService& service()
{
    static HistoryService history;
    return history;
}
}

namespace ALExposureSettingsHistory
{
const TrackedSetting* trackedSettings() { return TRACKED_SETTINGS; }
std::size_t trackedSettingCount() { return std::size(TRACKED_SETTINGS); }

const TrackedSetting* findTrackedSetting(const std::string& key)
{
    const auto found = std::find_if(std::begin(TRACKED_SETTINGS), std::end(TRACKED_SETTINGS),
        [&key](const TrackedSetting& setting) { return key == setting.mKey; });
    return found == std::end(TRACKED_SETTINGS) ? nullptr : &*found;
}

void initialize() { (void)service(); }
void clear() { service().clear(); }
void beginEdit() { service().beginEdit(); }
void endEdit() { service().endEdit(); }

void installEditFinalizers(LLView* root)
{
    if (!root)
    {
        return;
    }

    for (auto view = root->beginTreeBFS(); view != root->endTreeBFS(); ++view)
    {
        if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(*view))
        {
            slider->setSliderMouseDownCallback(
                [](LLUICtrl*, const LLSD&) { beginEdit(); });
            slider->setSliderMouseUpCallback(
                [](LLUICtrl*, const LLSD&) { endEdit(); });
            slider->setSliderEditorCommitCallback(
                [](LLUICtrl*, const LLSD&) { endEdit(); });
        }
        else if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(*view))
        {
            // Spinner arrows commit repeatedly while held. Finalize the group
            // from release, not from each repeat; typed values finalize on commit.
            for (LLView* child : *spinner->getChildList())
            {
                if (LLButton* button = dynamic_cast<LLButton*>(child))
                {
                    button->setMouseDownCallback(
                        [](LLUICtrl*, const LLSD&) { beginEdit(); });
                    button->setMouseUpCallback(
                        [](LLUICtrl*, const LLSD&) { endEdit(); });
                }
                else if (LLLineEditor* editor = dynamic_cast<LLLineEditor*>(child))
                {
                    editor->setCommitCallback(
                        [](LLUICtrl*, const LLSD&) { endEdit(); });
                }
            }
        }
    }
}

bool canUndo() { return service().model().canUndo(); }
bool canRedo() { return service().model().canRedo(); }
void undo() { service().undo(); }
void redo() { service().redo(); }
std::string undoLabel() { return service().model().undoLabel(); }
std::string redoLabel() { return service().model().redoLabel(); }
U64 revision() { return service().revision(); }
U64 pageStepSequence(LLView* page, bool redo) { return service().pageModel(page).stepSequence(redo); }
void replayPage(LLView* page, bool redo) { service().replayPage(page, redo); }
bool pageReplayReady(LLView* page, bool redo) { return service().pageReplayReady(page, redo); }
void discardPageStep(LLView* page, bool redo) { service().discardPageStep(page, redo); }
void finishPage(LLView* page) { service().finishPage(page); }
void discardPageRedo(LLView* page) { service().discardPageRedo(page); }

boost::signals2::connection connectRevisionChanged(const revision_signal_t::slot_type& slot)
{
    return service().signal().connect(slot);
}

ScopedBatch::ScopedBatch(const std::string& label)
{
    service().beginBatch(label);
}

ScopedBatch::~ScopedBatch()
{
    service().endBatch();
}
}
