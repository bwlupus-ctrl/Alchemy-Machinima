/**
 * Login-screen UI draw performance harness.
 */
#include "llviewerprecompiledheaders.h"

#include "aluiperfselftest.h"

#include "llappviewer.h"
#include "lldir.h"
#include "llerrorcontrol.h"
#include "llfile.h"
#include "llfloaterreg.h"
#include "llscrollcontainer.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "llviewercontrol.h"
#include "llview.h"

#include <algorithm>
#include <iomanip>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>

namespace
{
struct FrameSample
{
    F64 mUiMs = 0.0;
    U32 mDrawn = 0;
    U32 mCulled = 0;
    U32 mFindCalls = 0;
    U32 mFindVisited = 0;
};

struct TabTarget
{
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mChain;
    LLHandle<LLPanel> mPanel;
    std::string mPath;
    size_t mScrollBegin = 0;
    size_t mScrollEnd = 0;
};

struct ScrollTarget
{
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mChain;
    LLHandle<LLScrollContainer> mScroller;
    std::string mPath;
};

struct ResultRow
{
    std::string mPass;
    std::string mFloater;
    std::string mPath;
    std::string mMeasure;
    U32 mFrames = 0;
    F64 mMean = 0.0;
    F64 mP95 = 0.0;
    F64 mMax = 0.0;
    F64 mDrawn = 0.0;
    F64 mCulled = 0.0;
    F64 mFindCalls = 0.0;
    F64 mFindVisited = 0.0;
    F64 mActionMs = 0.0;
    F64 mFirstFrameMs = 0.0;
};

bool sActive = false;
bool sSuppressSettingsSave = false;
F64 sPendingUiMs = 0.0;
U32 sPendingUiCalls = 0;
U32 sPendingDrawn = 0;
U32 sPendingCulled = 0;
U32 sPendingFindCalls = 0;
U32 sPendingFindVisited = 0;

std::string csv(const std::string& value)
{
    std::string escaped = value;
    LLStringUtil::replaceString(escaped, "\"", "\"\"");
    return "\"" + escaped + "\"";
}

void collectEmbeddedCoverage(LLView* view, std::set<std::string>& coverage)
{
    if (!view)
    {
        return;
    }
    const std::string& name = view->getName();
    for (const char* marker : { "atmospheric_volumes", "vcam", "dof", "lens_looks" })
    {
        if (name.find(marker) != std::string::npos)
        {
            coverage.insert(marker);
        }
    }
    for (LLView* child : *view->getChildList())
    {
        collectEmbeddedCoverage(child, coverage);
    }
}

std::string tabPath(const std::string& raw_path, LLPanel* panel)
{
    std::set<std::string> coverage;
    collectEmbeddedCoverage(panel, coverage);
    if (coverage.empty())
    {
        return raw_path;
    }
    std::ostringstream out;
    out << raw_path << " [";
    bool first = true;
    for (const std::string& marker : coverage)
    {
        if (!first) out << ',';
        first = false;
        out << marker;
    }
    out << ']';
    return out.str();
}

void collectTabs(LLView* view,
                 const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& ancestors,
                 const std::string& prefix,
                 std::vector<TabTarget>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(child))
        {
            for (S32 i = 0; i < tabs->getTabCount(); ++i)
            {
                LLPanel* panel = tabs->getPanelByIndex(i);
                if (!panel)
                {
                    continue;
                }
                TabTarget target;
                target.mChain = ancestors;
                target.mChain.emplace_back(
                    tabs->getDerivedHandle<LLTabContainer>(), i);
                target.mPanel = panel->getHandle();
                const std::string raw_path = prefix.empty()
                    ? panel->getName() : prefix + "/" + panel->getName();
                target.mPath = tabPath(raw_path, panel);
                out.push_back(target);
                collectTabs(panel, target.mChain, raw_path, out);
            }
        }
        else
        {
            collectTabs(child, ancestors, prefix, out);
        }
    }
}

void collectScrollers(LLView* view,
                      const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain,
                      const std::string& prefix,
                      std::vector<ScrollTarget>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        // Nested tab pages get their own target/ancestor chain.
        if (dynamic_cast<LLTabContainer*>(child))
        {
            continue;
        }
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(child))
        {
            out.push_back({ chain,
                scroller->getDerivedHandle<LLScrollContainer>(),
                prefix.empty() ? scroller->getName()
                               : prefix + "/" + scroller->getName() });
        }
        collectScrollers(child, chain, prefix, out);
    }
}

bool applyChain(
    const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain)
{
    for (const auto& entry : chain)
    {
        LLTabContainer* tabs = entry.first.get();
        if (!tabs)
        {
            return false;
        }
        tabs->selectTab(entry.second);
    }
    return true;
}

bool chainAlive(
    const std::vector<std::pair<LLHandle<LLTabContainer>, S32>>& chain)
{
    return std::all_of(chain.begin(), chain.end(),
        [](const auto& entry) { return entry.first.get() != nullptr; });
}

void collectAllScrollers(LLView* view,
                         std::vector<LLHandle<LLScrollContainer>>& out)
{
    if (!view)
    {
        return;
    }
    for (LLView* child : *view->getChildList())
    {
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(child))
        {
            out.push_back(scroller->getDerivedHandle<LLScrollContainer>());
        }
        collectAllScrollers(child, out);
    }
}

class Runner
{
public:
    Runner(std::string request, U32 frames, bool ab)
    : mRequest(std::move(request)),
      mFrames(llmax<U32>(frames, 2)),
      mAB(ab),
      mPriorCull(gSavedSettings.getBOOL("UIClipCullDraw"))
    {
        static const std::vector<std::string> all_targets = {
            "director", "lightbox", "exposure_hub", "dance_ball",
            "cine_light_rig", "env_adjust_snapshot", "actor_gaze",
            "avatar_light_response", "flycam_recorder", "phototools",
            "cinematic_camera", "ghost_studio", "ultimate_diopter",
            "pose_polish"
        };
        if (mRequest == "all")
        {
            mTargets = all_targets;
        }
        else
        {
            LLStringUtil::getTokens(mRequest, mTargets, ",");
            for (std::string& target : mTargets)
            {
                LLStringUtil::trim(target);
            }
        }

        mCullOn = mAB ? true : mPriorCull;
        if (mAB)
        {
            gSavedSettings.setBOOL("UIClipCullDraw", true);
        }

        for (const std::string& target : mTargets)
        {
            mWasVisible[target] = LLFloaterReg::instanceVisible(target);
            if (mWasVisible[target])
            {
                LLFloaterReg::hideInstance(target);
            }
        }
        mSettle.reset();
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest waiting 3 seconds at login screen" << LL_ENDL;
    }

    // No restore() here: the static owner can be destroyed after viewer cleanup has
    // torn down floaters/settings. FINISH and abortForLogin() restore explicitly.
    ~Runner() = default;

    // The user started a real login mid-test: put everything back and stop.
    void abortForLogin()
    {
        LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest aborted: login started" << LL_ENDL;
        restoreInitialState();
        restore();
        sActive = false;
        sSuppressSettingsSave = false;
    }

    bool tick() { return step(); }

private:
    enum Phase
    {
        WAIT_SETTLE,
        BASELINE,
        OPEN,
        FIRST_FRAME,
        DEFAULT_MEASURE,
        TAB_SELECT,
        TAB_FIRST_FRAME,
        TAB_MEASURE,
        SCROLL_BOTTOM,
        SCROLL_BOTTOM_MEASURE,
        SCROLL_TOP,
        SCROLL_TOP_MEASURE,
        CLOSE,
        CLOSE_SETTLE,
        NEXT_PASS,
        FINISH
    };

    bool takeFrame(FrameSample& sample)
    {
        if (!sPendingUiCalls)
        {
            return false;
        }
        sample.mUiMs = sPendingUiMs;
        sample.mDrawn = sPendingDrawn;
        sample.mCulled = sPendingCulled;
        sample.mFindCalls = sPendingFindCalls;
        sample.mFindVisited = sPendingFindVisited;
        sPendingUiMs = 0.0;
        sPendingUiCalls = 0;
        sPendingDrawn = sPendingCulled = 0;
        sPendingFindCalls = sPendingFindVisited = 0;
        mUiFrameStall.reset();
        mUiFrameStallWarned = false;
        return true;
    }

    bool collect(U32 wanted)
    {
        FrameSample sample;
        if (!takeFrame(sample))
        {
            return false;
        }
        mSamples.push_back(sample);
        return mSamples.size() >= wanted;
    }

    void addRow(const std::string& floater, const std::string& path,
                const std::string& measure, F64 action_ms = 0.0,
                F64 first_frame_ms = 0.0)
    {
        ResultRow row;
        row.mPass = mCullOn ? "clip_on" : "clip_off";
        row.mFloater = floater;
        row.mPath = path;
        row.mMeasure = measure;
        row.mFrames = (U32)mSamples.size();
        row.mActionMs = action_ms;
        row.mFirstFrameMs = first_frame_ms;
        if (!mSamples.empty())
        {
            std::vector<F64> times;
            times.reserve(mSamples.size());
            for (const FrameSample& sample : mSamples)
            {
                times.push_back(sample.mUiMs);
                row.mDrawn += sample.mDrawn;
                row.mCulled += sample.mCulled;
                row.mFindCalls += sample.mFindCalls;
                row.mFindVisited += sample.mFindVisited;
            }
            std::sort(times.begin(), times.end());
            row.mMean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
            row.mP95 = times[(times.size() - 1) * 95 / 100];
            row.mMax = times.back();
            row.mDrawn /= times.size();
            row.mCulled /= times.size();
            row.mFindCalls /= times.size();
            row.mFindVisited /= times.size();
        }
        mRows.push_back(row);
        mSamples.clear();
    }

    void prepareFloaterActions()
    {
        mTabs.clear();
        mDefaultScrollers.clear();
        mScrollers.clear();
        LLFloater* floater = mFloater.get();
        if (!floater)
        {
            return;
        }
        collectScrollers(floater, {}, "default", mDefaultScrollers);
        collectTabs(floater, {}, "", mTabs);
        for (TabTarget& tab : mTabs)
        {
            tab.mScrollBegin = mScrollers.size();
            collectScrollers(tab.mPanel.get(), tab.mChain, tab.mPath, mScrollers);
            tab.mScrollEnd = mScrollers.size();
        }
        mTabIndex = 0;
        mScrollIndex = 0;
        mInDefaultScroll = false;

        mInitialTabs.clear();
        std::set<LLTabContainer*> seen_tabs;
        for (const TabTarget& target : mTabs)
        {
            for (const auto& entry : target.mChain)
            {
                LLTabContainer* tabs = entry.first.get();
                if (tabs && seen_tabs.insert(tabs).second)
                {
                    mInitialTabs.emplace_back(
                        entry.first, tabs->getCurrentPanelIndex());
                }
            }
        }
        mInitialScroll.clear();
        std::vector<LLHandle<LLScrollContainer>> all_scrollers;
        collectAllScrollers(floater, all_scrollers);
        for (const LLHandle<LLScrollContainer>& handle : all_scrollers)
        {
            if (LLScrollContainer* scroller = handle.get())
            {
                mInitialScroll.emplace_back(
                    handle, scroller->getDocPosVertical());
            }
        }
    }

    void restoreInitialState()
    {
        for (const auto& entry : mInitialTabs)
        {
            if (LLTabContainer* tabs = entry.first.get())
            {
                tabs->selectTab(entry.second);
            }
        }
        for (const auto& entry : mInitialScroll)
        {
            if (LLScrollContainer* scroller = entry.first.get())
            {
                scroller->setDocPosVertical(entry.second);
            }
        }
    }

    void closeCurrentTarget()
    {
        restoreInitialState();
        if (mFloater.get())
        {
            LLFloaterReg::hideInstance(mTargets[mTargetIndex]);
        }
        mFloater = LLHandle<LLFloater>();
        mSamples.clear();
        mSettleFrames = 0;
        mPhase = CLOSE_SETTLE;
    }

    void abortCurrentTarget(const std::string& reason)
    {
        LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest abort "
            << mTargets[mTargetIndex] << ": " << reason << LL_ENDL;
        closeCurrentTarget();
    }

    bool nextTarget()
    {
        return ++mTargetIndex < (S32)mTargets.size();
    }

    bool step()
    {
        if (mUiFrameStall.getElapsedTimeF32() >= 10.f &&
            !mUiFrameStallWarned)
        {
            LL_WARNS("UIPerfSelfTest")
                << "UIPerfSelfTest no UI frame for 10 s: window hidden or minimized"
                << LL_ENDL;
            mUiFrameStallWarned = true;
        }
        if (mPhase >= FIRST_FRAME && mPhase <= CLOSE && !mFloater.get())
        {
            abortCurrentTarget("floater died");
            return false;
        }
        if (mPhase >= TAB_SELECT && mPhase <= TAB_MEASURE &&
            (mTabIndex >= mTabs.size() || !mTabs[mTabIndex].mPanel.get() ||
             !chainAlive(mTabs[mTabIndex].mChain)))
        {
            abortCurrentTarget("tab target died");
            return false;
        }
        if (mPhase >= SCROLL_BOTTOM && mPhase <= SCROLL_TOP_MEASURE)
        {
            const std::vector<ScrollTarget>& targets = mInDefaultScroll
                ? mDefaultScrollers : mScrollers;
            if (mScrollIndex >= targets.size() ||
                !targets[mScrollIndex].mScroller.get() ||
                !chainAlive(targets[mScrollIndex].mChain))
            {
                abortCurrentTarget("scroller target died");
                return false;
            }
            if (!mInDefaultScroll &&
                (mTabIndex >= mTabs.size() || !mTabs[mTabIndex].mPanel.get()))
            {
                abortCurrentTarget("tab panel died");
                return false;
            }
        }
        switch (mPhase)
        {
        case WAIT_SETTLE:
        {
            FrameSample ignored;
            takeFrame(ignored);
            if (mSettle.getElapsedTimeF32() < 3.f) return false;
            mPhase = BASELINE;
            break;
        }
        case BASELINE:
            if (collect(mFrames))
            {
                addRow("", "", "baseline");
                mTargetIndex = -1;
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
            }
            break;
        case OPEN:
        {
            const std::string& name = mTargets[mTargetIndex];
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin " << name << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            LLTimer timer;
            LLFloater* floater = LLFloaterReg::showInstance(name, LLSD(), false);
            mActionMs = timer.getElapsedTimeF64() * 1000.0;
            if (!floater)
            {
                LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest skip " << name
                    << ": registry name did not produce a floater" << LL_ENDL;
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
                break;
            }
            mFloater = floater->getHandle();
            prepareFloaterActions();
            mPhase = FIRST_FRAME;
            break;
        }
        case FIRST_FRAME:
        {
            FrameSample sample;
            if (!takeFrame(sample)) return false;
            addRow(mTargets[mTargetIndex], "", "open", mActionMs, sample.mUiMs);
            mPhase = DEFAULT_MEASURE;
            break;
        }
        case DEFAULT_MEASURE:
            if (collect(mFrames))
            {
                addRow(mTargets[mTargetIndex], "default", "steady");
                if (!mDefaultScrollers.empty())
                {
                    mInDefaultScroll = true;
                    mScrollIndex = 0;
                    mPhase = SCROLL_BOTTOM;
                }
                else
                {
                    mPhase = mTabs.empty() ? CLOSE : TAB_SELECT;
                }
            }
            break;
        case TAB_SELECT:
        {
            const TabTarget& target = mTabs[mTabIndex];
            if (!target.mPanel.get())
            {
                abortCurrentTarget("tab panel died: " + target.mPath);
                break;
            }
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin "
                << mTargets[mTargetIndex] << "/" << target.mPath << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            LLTimer timer;
            if (!applyChain(target.mChain))
            {
                abortCurrentTarget("tab container died: " + target.mPath);
                break;
            }
            mActionMs = timer.getElapsedTimeF64() * 1000.0;
            mPhase = TAB_FIRST_FRAME;
            break;
        }
        case TAB_FIRST_FRAME:
        {
            FrameSample sample;
            if (!takeFrame(sample)) return false;
            mFirstFrameMs = sample.mUiMs;
            mPhase = TAB_MEASURE;
            break;
        }
        case TAB_MEASURE:
            if (collect(mFrames / 2))
            {
                const TabTarget& target = mTabs[mTabIndex];
                addRow(mTargets[mTargetIndex], target.mPath, "tab",
                       mActionMs, mFirstFrameMs);
                mScrollIndex = target.mScrollBegin;
                mPhase = mScrollIndex < target.mScrollEnd
                    ? SCROLL_BOTTOM : (++mTabIndex < mTabs.size() ? TAB_SELECT : CLOSE);
            }
            break;
        case SCROLL_BOTTOM:
        {
            FrameSample ignored;
            takeFrame(ignored);
            ScrollTarget& target = mInDefaultScroll
                ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
            LLScrollContainer* scroller = target.mScroller.get();
            if (!scroller || !applyChain(target.mChain))
            {
                abortCurrentTarget("scroller died: " + target.mPath);
                break;
            }
            scroller->goToBottom();
            mPhase = SCROLL_BOTTOM_MEASURE;
            break;
        }
        case SCROLL_BOTTOM_MEASURE:
            if (collect(mFrames / 2))
            {
                const ScrollTarget& target = mInDefaultScroll
                    ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
                addRow(mTargets[mTargetIndex], target.mPath,
                       "scroll_bottom");
                mPhase = SCROLL_TOP;
            }
            break;
        case SCROLL_TOP:
        {
            FrameSample ignored;
            takeFrame(ignored);
            ScrollTarget& target = mInDefaultScroll
                ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
            LLScrollContainer* scroller = target.mScroller.get();
            if (!scroller)
            {
                abortCurrentTarget("scroller died: " + target.mPath);
                break;
            }
            scroller->goToTop();
            mPhase = SCROLL_TOP_MEASURE;
            break;
        }
        case SCROLL_TOP_MEASURE:
            if (collect(mFrames / 2))
            {
                const ScrollTarget& target = mInDefaultScroll
                    ? mDefaultScrollers[mScrollIndex] : mScrollers[mScrollIndex];
                addRow(mTargets[mTargetIndex], target.mPath,
                       "scroll_top");
                ++mScrollIndex;
                if (mInDefaultScroll)
                {
                    if (mScrollIndex < mDefaultScrollers.size())
                    {
                        mPhase = SCROLL_BOTTOM;
                    }
                    else
                    {
                        mInDefaultScroll = false;
                        mScrollIndex = 0;
                        mPhase = mTabs.empty() ? CLOSE : TAB_SELECT;
                    }
                    break;
                }
                if (mScrollIndex < mTabs[mTabIndex].mScrollEnd)
                {
                    mPhase = SCROLL_BOTTOM;
                }
                else
                {
                    ++mTabIndex;
                    mPhase = mTabIndex < mTabs.size() ? TAB_SELECT : CLOSE;
                }
            }
            break;
        case CLOSE:
            closeCurrentTarget();
            break;
        case CLOSE_SETTLE:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mSettleFrames >= 30)
            {
                mPhase = nextTarget() ? OPEN : NEXT_PASS;
            }
            break;
        }
        case NEXT_PASS:
            if (mAB && mCullOn)
            {
                FrameSample ignored;
                takeFrame(ignored);
                mCullOn = false;
                // ui-draw-lint: allow(setting-write) one-shot self-test toggle, login screen only
                gSavedSettings.setBOOL("UIClipCullDraw", false);
                mSamples.clear();
                mPhase = BASELINE;
            }
            else
            {
                mPhase = FINISH;
            }
            break;
        case FINISH:
            writeResults();
            restore();
            sActive = false;
            LLAppViewer::instance()->requestQuit();
            return true;
        }
        return false;
    }

    void writeResults()
    {
        const std::string path = gDirUtilp->getExpandedFilename(
            LL_PATH_LOGS, "ui_perf_selftest.csv");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << "pass,floater,tab_path,measure,frames,ui_mean_ms,ui_p95_ms,ui_max_ms,"
               "views_drawn_mean,views_culled_mean,find_calls_mean,find_visited_mean,"
               "action_ms,first_frame_ui_ms\n";
        out << std::fixed << std::setprecision(4);
        for (const ResultRow& row : mRows)
        {
            out << csv(row.mPass) << ',' << csv(row.mFloater) << ','
                << csv(row.mPath) << ',' << csv(row.mMeasure) << ','
                << row.mFrames << ',' << row.mMean << ',' << row.mP95 << ','
                << row.mMax << ',' << row.mDrawn << ',' << row.mCulled << ','
                << row.mFindCalls << ',' << row.mFindVisited << ','
                << row.mActionMs << ',' << row.mFirstFrameMs << '\n';
        }
        out.flush();
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest summary: wrote "
            << mRows.size() << " rows to " << path << LL_ENDL;
        for (const ResultRow& row : mRows)
        {
            if (row.mMeasure == "baseline" || row.mMeasure == "steady" ||
                row.mMeasure == "tab")
            {
                LL_INFOS("UIPerfSelfTest") << row.mPass << " " << row.mFloater
                    << " " << row.mPath << " " << row.mMeasure
                    << " mean=" << row.mMean << " p95=" << row.mP95
                    << " max=" << row.mMax << LL_ENDL;
            }
        }
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest summary complete" << LL_ENDL;
    }

    void restore()
    {
        if (mRestored) return;
        mRestored = true;
        for (const std::string& target : mTargets)
        {
            LLFloaterReg::hideInstance(target);
        }
        for (const auto& saved : mWasVisible)
        {
            if (saved.second)
            {
                LLFloaterReg::showInstance(saved.first, LLSD(), false);
            }
        }
        // ui-draw-lint: allow(setting-write) one-shot self-test toggle, login screen only
        gSavedSettings.setBOOL("UIClipCullDraw", mPriorCull);
    }

    std::string mRequest;
    U32 mFrames;
    bool mAB;
    bool mPriorCull;
    bool mCullOn = true;
    bool mRestored = false;
    Phase mPhase = WAIT_SETTLE;
    LLFrameTimer mSettle;
    std::vector<std::string> mTargets;
    std::map<std::string, bool> mWasVisible;
    std::vector<ResultRow> mRows;
    std::vector<FrameSample> mSamples;
    LLHandle<LLFloater> mFloater;
    std::vector<TabTarget> mTabs;
    std::vector<ScrollTarget> mDefaultScrollers;
    std::vector<ScrollTarget> mScrollers;
    std::vector<std::pair<LLHandle<LLTabContainer>, S32>> mInitialTabs;
    std::vector<std::pair<LLHandle<LLScrollContainer>, S32>> mInitialScroll;
    S32 mTargetIndex = -1;
    size_t mTabIndex = 0;
    size_t mScrollIndex = 0;
    bool mInDefaultScroll = false;
    U32 mSettleFrames = 0;
    F64 mActionMs = 0.0;
    F64 mFirstFrameMs = 0.0;
    LLFrameTimer mUiFrameStall;
    bool mUiFrameStallWarned = false;
};

static std::unique_ptr<Runner> sRunner;
}

void ALUIPerfSelfTest::startIfRequested()
{
    static LLCachedControl<std::string> request(
        gSavedSettings, "UIPerfSelfTest", std::string());
    if (request().empty() || sActive)
    {
        return;
    }
    static LLCachedControl<U32> frames(gSavedSettings, "UIPerfSelfTestFrames", 120);
    static LLCachedControl<bool> ab(gSavedSettings, "UIPerfSelfTestAB", false);
    sActive = true;
    sSuppressSettingsSave = true;
    sRunner = std::make_unique<Runner>(request(), frames, ab);
}

void ALUIPerfSelfTest::tick()
{
    if (!sRunner)
    {
        return;
    }
    if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT)
    {
        sRunner->abortForLogin();
        sRunner.reset();
        return;
    }
    if (sRunner->tick())
    {
        sRunner.reset();
    }
}

void ALUIPerfSelfTest::recordUIDraw(F64 milliseconds)
{
    if (sActive)
    {
        sPendingUiMs += milliseconds;
        ++sPendingUiCalls;
    }
}

void ALUIPerfSelfTest::recordViewCounts(U32 drawn, U32 culled,
                                        U32 find_calls, U32 find_visited)
{
    if (sActive)
    {
        sPendingDrawn += drawn;
        sPendingCulled += culled;
        sPendingFindCalls += find_calls;
        sPendingFindVisited += find_visited;
    }
}

bool ALUIPerfSelfTest::suppressSettingsSave()
{
    return sSuppressSettingsSave;
}

bool ALUIPerfSelfTest::isActive()
{
    return sActive;
}
