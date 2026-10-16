/**
 * @file alsettingsundohistorymodel.cpp
 * @brief Pure model for a bounded, coalescing settings undo history.
 */

#include "linden_common.h"
#include "alsettingsundohistorymodel.h"
#include "alundosequence.h"
#include "llsdutil.h"

#include <algorithm>
#include <utility>

void ALSettingsUndoHistoryModel::record(
    const std::string& key, const std::string& label,
    const LLSD& old_value, const LLSD& new_value, U64 now_ms)
{
    if (old_value == new_value)
    {
        return;
    }

    Change change{ key, label, old_value, new_value };

    if (mBatchDepth > 0)
    {
        mergeChange(mBatch, change);
        mBatch.mSequence = alNextUndoSequence();
        changed();
        return;
    }

    if (!mPending.mChanges.empty())
    {
        const bool same_key = mPending.mChanges.size() == 1 &&
                              mPending.mChanges.front().mKey == key;
        const bool in_window = mEditActive ||
                               (now_ms >= mLastChangeMs &&
                                now_ms - mLastChangeMs <= COALESCE_MILLISECONDS);
        if (same_key && in_window)
        {
            mergeChange(mPending, change);
            mPending.mSequence = alNextUndoSequence();
            mLastChangeMs = now_ms;
            changed();
            return;
        }
        commitPending();
    }

    mPending.mLabel = label;
    mPending.mSequence = alNextUndoSequence();
    mPending.mChanges.push_back(std::move(change));
    mLastChangeMs = now_ms;
    changed();
}

void ALSettingsUndoHistoryModel::beginBatch(const std::string& label)
{
    if (mBatchDepth++ == 0)
    {
        commitPending();
        mBatch = Step();
        mBatch.mLabel = label;
    }
}

void ALSettingsUndoHistoryModel::endBatch()
{
    if (mBatchDepth == 0)
    {
        return;
    }
    if (--mBatchDepth == 0 && !mBatch.mChanges.empty())
    {
        pushUndo(std::move(mBatch), true);
        mBatch = Step();
    }
}

void ALSettingsUndoHistoryModel::beginEdit()
{
    if (mBatchDepth == 0 && !mEditActive)
    {
        commitPending();
        mEditActive = true;
    }
}

void ALSettingsUndoHistoryModel::endEdit()
{
    if (mBatchDepth == 0)
    {
        mEditActive = false;
        commitPending();
    }
}

void ALSettingsUndoHistoryModel::closeIdle(U64 now_ms)
{
    if (!mEditActive && !mPending.mChanges.empty() && now_ms >= mLastChangeMs &&
        now_ms - mLastChangeMs >= COALESCE_MILLISECONDS)
    {
        commitPending();
    }
}

bool ALSettingsUndoHistoryModel::canUndo() const
{
    return !mPending.mChanges.empty() || !mUndo.empty();
}

bool ALSettingsUndoHistoryModel::canRedo() const
{
    return mBatchDepth == 0 && mPending.mChanges.empty() && !mRedo.empty();
}

bool ALSettingsUndoHistoryModel::replayReady(bool redo, const replay_filter_t& filter) const
{
    if (mBatchDepth != 0 || (redo ? !canRedo() : !canUndo()))
        return false;
    const Step& step = !redo && !mPending.mChanges.empty() ? mPending :
        (redo ? mRedo.back() : mUndo.back());
    return std::all_of(step.mChanges.begin(), step.mChanges.end(), filter);
}

void ALSettingsUndoHistoryModel::discardStep(bool redo)
{
    Step step;
    const auto reject = [](const Change&) { return false; };
    if (redo) takeRedo(step, reject);
    else takeUndo(step, reject);
}

bool ALSettingsUndoHistoryModel::takeUndo(Step& step, const replay_filter_t& filter)
{
    if (mBatchDepth != 0)
    {
        return false;
    }
    mEditActive = false;
    commitPending();
    if (mUndo.empty())
    {
        return false;
    }
    step = std::move(mUndo.back());
    mUndo.pop_back();
    if (filter)
        std::erase_if(step.mChanges, [&filter](const Change& change) { return !filter(change); });
    if (!step.mChanges.empty())
    {
        mRedo.push_back(step);
        mRedo.back().mSequence = alNextUndoSequence();
    }
    changed();
    return true;
}

bool ALSettingsUndoHistoryModel::takeRedo(Step& step, const replay_filter_t& filter)
{
    if (!canRedo())
    {
        return false;
    }
    step = std::move(mRedo.back());
    mRedo.pop_back();
    if (filter)
        std::erase_if(step.mChanges, [&filter](const Change& change) { return !filter(change); });
    if (!step.mChanges.empty())
    {
        pushUndo(Step(step), false);
        mUndo.back().mSequence = alNextUndoSequence();
    }
    changed();
    return true;
}

void ALSettingsUndoHistoryModel::clear()
{
    const bool had_state = !mUndo.empty() || !mRedo.empty() ||
                           !mPending.mChanges.empty() || !mBatch.mChanges.empty() ||
                           mBatchDepth != 0 || mEditActive;
    mUndo.clear();
    mRedo.clear();
    mPending = Step();
    mBatch = Step();
    mLastChangeMs = 0;
    mBatchDepth = 0;
    mEditActive = false;
    if (had_state)
    {
        changed();
    }
}

void ALSettingsUndoHistoryModel::discardRedo()
{
    if (!mRedo.empty())
    {
        mRedo.clear();
        changed();
    }
}

U64 ALSettingsUndoHistoryModel::stepSequence(bool redo) const
{
    if (!redo && !mPending.mChanges.empty())
        return mPending.mSequence;
    const auto& steps = redo ? mRedo : mUndo;
    return (redo && !canRedo()) || steps.empty() ? 0 : steps.back().mSequence;
}

std::string ALSettingsUndoHistoryModel::undoLabel() const
{
    if (!mPending.mChanges.empty())
    {
        return mPending.mLabel;
    }
    return mUndo.empty() ? std::string() : mUndo.back().mLabel;
}

std::string ALSettingsUndoHistoryModel::redoLabel() const
{
    return mRedo.empty() ? std::string() : mRedo.back().mLabel;
}

void ALSettingsUndoHistoryModel::commitPending()
{
    if (!mPending.mChanges.empty())
    {
        pushUndo(std::move(mPending), true);
        mPending = Step();
    }
}

void ALSettingsUndoHistoryModel::pushUndo(Step&& step, bool clear_redo)
{
    if (step.mChanges.empty())
    {
        return;
    }
    if (clear_redo)
    {
        mRedo.clear();
    }
    mUndo.push_back(std::move(step));
    if (mUndo.size() > MAX_DEPTH)
    {
        mUndo.erase(mUndo.begin());
    }
    changed();
}

void ALSettingsUndoHistoryModel::mergeChange(Step& step, const Change& change)
{
    auto found = std::find_if(step.mChanges.begin(), step.mChanges.end(),
        [&change](const Change& existing) { return existing.mKey == change.mKey; });
    if (found == step.mChanges.end())
    {
        step.mChanges.push_back(change);
        return;
    }
    found->mNewValue = change.mNewValue;
    if (found->mOldValue == found->mNewValue)
    {
        step.mChanges.erase(found);
    }
}

void ALSettingsUndoHistoryModel::changed()
{
    ++mRevision;
}
