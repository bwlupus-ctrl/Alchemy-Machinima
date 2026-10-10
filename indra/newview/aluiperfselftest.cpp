/**
 * Login-screen UI draw performance harness.
 */
#include "llviewerprecompiledheaders.h"

#include "aluiperfselftest.h"

#include "llappviewer.h"
#include "llaccordionctrl.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llerrorcontrol.h"
#include "llfile.h"
#include "llfloaterreg.h"
#include "lliconctrl.h"
#include "llimage.h"
#include "llimagepng.h"
#include "lllineeditor.h"
#include "llmultisliderctrl.h"
#include "llpanel.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstartup.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llview.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <numeric>
#include <regex>
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

struct LayoutFinding
{
    std::string mSizePass;
    std::string mFloater;
    U32 mPageIndex = 0;
    std::string mPath;
    std::string mFinding;
    std::string mWidgetA;
    std::string mWidgetB;
    LLRect mRectA;
    LLRect mRectB;
    std::string mDetail;
};

struct AuditLeaf
{
    LLView* mView = nullptr;
    LLScrollContainer* mContainingScroller = nullptr;
    LLRect mRect;
    std::string mPath;
    std::set<std::string> mLineage;
};

using NamePair = std::pair<std::string, std::string>;

NamePair orderedPair(std::string a, std::string b)
{
    return a < b ? NamePair(std::move(a), std::move(b))
                 : NamePair(std::move(b), std::move(a));
}

struct OverlapPolicy
{
    std::set<NamePair> mAllowedPairs;
    std::vector<std::set<std::string>> mExclusiveGroups;

    bool allows(const AuditLeaf& a, const AuditLeaf& b) const
    {
        for (const std::string& aname : a.mLineage)
        {
            for (const std::string& bname : b.mLineage)
            {
                if (mAllowedPairs.count(orderedPair(aname, bname)))
                {
                    return true;
                }
                for (const std::set<std::string>& group : mExclusiveGroups)
                {
                    if (aname != bname && group.count(aname) && group.count(bname))
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

std::vector<std::string> splitWords(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '<' || ch == '>') ch = ' ';
    }
    std::vector<std::string> words;
    LLStringUtil::getTokens(value, words, " \t\r\n");
    words.erase(std::remove_if(words.begin(), words.end(),
        [](const std::string& word) { return word == "--" || word == "-->"; }),
        words.end());
    return words;
}

OverlapPolicy loadOverlapPolicy()
{
    OverlapPolicy policy;
    std::set<std::string> directories;
    const std::string delimiter = gDirUtilp->getDirDelimiter();
    for (const std::string& root : { gDirUtilp->getDefaultSkinDir(),
                                     gDirUtilp->getSkinDir(),
                                     gDirUtilp->getUserDefaultSkinDir(),
                                     gDirUtilp->getUserSkinDir() })
    {
        if (!root.empty())
        {
            directories.insert(root + delimiter + "xui" + delimiter + "en");
        }
    }
    const std::regex annotation(
        R"(xui-check:\s*(allow-overlap|overlay|exclusive|independent)\s+([^<]*))",
        std::regex::icase);
    for (const std::string& directory : directories)
    {
        LLDirIterator iterator(directory, "*.xml");
        std::string filename;
        while (iterator.next(filename))
        {
            llifstream input((directory + delimiter + filename).c_str());
            std::string line;
            while (std::getline(input, line))
            {
                if (line.find("xui-check") == std::string::npos) continue;
                std::smatch match;
                if (!std::regex_search(line, match, annotation)) continue;
                std::string kind = match[1].str();
                LLStringUtil::toLower(kind);
                const std::vector<std::string> names = splitWords(match[2].str());
                if ((kind == "allow-overlap" || kind == "overlay") && names.size() >= 2)
                {
                    policy.mAllowedPairs.insert(orderedPair(names[0], names[1]));
                }
                else if (kind == "exclusive" && names.size() >= 2)
                {
                    policy.mExclusiveGroups.emplace_back(names.begin(), names.end());
                }
                // independent deliberately creates no runtime exception: unlike
                // the static checker, this audit does not infer sibling motion.
            }
        }
    }
    return policy;
}

std::string rectString(const LLRect& rect)
{
    return llformat("[%d,%d,%d,%d]", rect.mLeft, rect.mBottom,
                    rect.mRight, rect.mTop);
}

std::string safeComponent(std::string value)
{
    for (char& ch : value)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!std::isalnum(uch) && ch != '-' && ch != '_') ch = '_';
    }
    while (value.find("__") != std::string::npos)
    {
        LLStringUtil::replaceString(value, "__", "_");
    }
    if (value.empty()) value = "default";
    if (value.size() > 100) value.resize(100);
    return value;
}

bool visibleWithin(LLView* view, LLView* page)
{
    for (LLView* cursor = view; cursor; cursor = cursor->getParent())
    {
        if (!cursor->getVisible()) return false;
        if (cursor == page) return true;
        if (cursor->getRect().getWidth() <= 0 || cursor->getRect().getHeight() <= 0)
        {
            return false;
        }
    }
    return false;
}

std::string relativePath(LLView* view, LLView* page)
{
    std::vector<std::string> parts;
    for (LLView* cursor = view; cursor && cursor != page; cursor = cursor->getParent())
    {
        parts.push_back(cursor->getName().empty() ? "<unnamed>" : cursor->getName());
    }
    std::reverse(parts.begin(), parts.end());
    std::ostringstream out;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i) out << '/';
        out << parts[i];
    }
    return out.str();
}

bool isAuditLeaf(LLView* view)
{
    if (dynamic_cast<LLPanel*>(view) || dynamic_cast<LLScrollContainer*>(view))
    {
        return false;
    }
    if (dynamic_cast<LLButton*>(view) || dynamic_cast<LLTextBox*>(view) ||
        dynamic_cast<LLIconCtrl*>(view) || dynamic_cast<LLScrollListCtrl*>(view))
    {
        return true;
    }
    return dynamic_cast<LLUICtrl*>(view) && view->getChildCount() == 0;
}

void collectAuditViews(LLView* root, LLView* page,
                       std::vector<LLView*>& views,
                       std::vector<AuditLeaf>& leaves)
{
    if (!root || !visibleWithin(root, page)) return;
    views.push_back(root);
    if (root != page && isAuditLeaf(root))
    {
        AuditLeaf leaf;
        leaf.mView = root;
        leaf.mPath = relativePath(root, page);
        root->localRectToOtherView(root->getLocalRect(), &leaf.mRect, page);
        if (leaf.mRect.getWidth() > 0 && leaf.mRect.getHeight() > 0)
        {
            for (LLView* cursor = root; cursor && cursor != page; cursor = cursor->getParent())
            {
                if (!cursor->getName().empty()) leaf.mLineage.insert(cursor->getName());
            }
            leaves.push_back(std::move(leaf));
        }
    }
    for (LLView* child : *root->getChildList())
    {
        if (LLTabContainer* tabs = dynamic_cast<LLTabContainer*>(root))
        {
            bool is_tab_page = false;
            for (S32 i = 0; i < tabs->getTabCount(); ++i)
            {
                if (child == tabs->getPanelByIndex(i))
                {
                    is_tab_page = true;
                    break;
                }
            }
            if (is_tab_page) continue;
        }
        // A scroll container's bars and corner are implementation chrome.  Its
        // document is the page content that the audit must inspect in full.
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(root))
        {
            if (child != scroller->getScrolledView()) continue;
        }
        collectAuditViews(child, page, views, leaves);
    }
}

LLView* nearestCommonAncestor(LLView* a, LLView* b, LLView* page)
{
    std::set<LLView*> ancestors;
    for (LLView* cursor = a->getParent(); cursor && cursor != page; cursor = cursor->getParent())
    {
        ancestors.insert(cursor);
    }
    for (LLView* cursor = b->getParent(); cursor && cursor != page; cursor = cursor->getParent())
    {
        if (ancestors.count(cursor)) return cursor;
    }
    return nullptr;
}

bool isRealCompositeControl(LLView* view)
{
    return dynamic_cast<LLUICtrl*>(view) &&
        !dynamic_cast<LLPanel*>(view) &&
        !dynamic_cast<LLScrollContainer*>(view) &&
        !dynamic_cast<LLTabContainer*>(view) &&
        !dynamic_cast<LLAccordionCtrl*>(view);
}

LLScrollContainer* containingScroller(LLView* view, LLFloater* floater)
{
    for (LLView* cursor = view->getParent(); cursor && cursor != floater;
         cursor = cursor->getParent())
    {
        if (LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(cursor))
        {
            LLView* document = scroller->getScrolledView();
            if (document && (view == document || view->hasAncestor(document)))
            {
                return scroller;
            }
        }
    }
    return nullptr;
}

void containingScrollAxes(LLView* view, LLFloater* floater,
                          bool& horizontal, bool& vertical)
{
    horizontal = false;
    vertical = false;
    for (LLView* cursor = view->getParent(); cursor && cursor != floater;
         cursor = cursor->getParent())
    {
        LLScrollContainer* scroller = dynamic_cast<LLScrollContainer*>(cursor);
        if (!scroller || !scroller->getScrolledView()) continue;
        const LLRect visible = scroller->getVisibleContentRect();
        const LLRect document = scroller->getScrolledView()->getLocalRect();
        horizontal = horizontal || document.getWidth() > visible.getWidth() + 1;
        vertical = vertical || document.getHeight() > visible.getHeight() + 1;
    }
}

S32 longestLineWidth(const LLFontGL* font, const std::string& text)
{
    S32 width = 0;
    size_t begin = 0;
    while (begin <= text.size())
    {
        const size_t end = text.find('\n', begin);
        width = llmax(width, font->getWidth(
            text.substr(begin, end == std::string::npos ? end : end - begin)));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return width;
}

LLRect overlapRect(const AuditLeaf& leaf)
{
    LLRect rect = leaf.mRect;
    LLTextBox* text = dynamic_cast<LLTextBox*>(leaf.mView);
    if (!text || !text->getFont() || text->getText().empty()) return rect;

    const S32 width = llmin(rect.getWidth(),
                            longestLineWidth(text->getFont(), text->getText()) +
                                text->getHPad());
    S32 left = rect.mLeft;
    if (text->getHAlign() == LLFontGL::RIGHT)
    {
        left = rect.mRight - width;
    }
    else if (text->getHAlign() == LLFontGL::HCENTER)
    {
        left = rect.mLeft + (rect.getWidth() - width) / 2;
    }
    rect.mLeft = llclamp(left, rect.mLeft, rect.mRight);
    rect.mRight = llclamp(rect.mLeft + width, rect.mLeft, rect.mRight);
    return rect;
}

bool sActive = false;
bool sSuppressSettingsSave = false;
bool sLayoutAuditActive = false;
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
    Runner(std::string request, U32 frames, bool ab, bool layout)
    : mRequest(std::move(request)),
      mFrames(llmax<U32>(frames, 2)),
      mAB(ab),
      mLayout(layout),
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
        if (mLayout)
        {
            mOverlapPolicy = loadOverlapPolicy();
            mAuditRoot = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "ui_audit");
            std::error_code remove_error;
            std::filesystem::remove_all(mAuditRoot, remove_error);
            if (remove_error)
            {
                LL_WARNS("UILayoutAudit") << "Unable to clear " << mAuditRoot
                    << ": " << remove_error.message() << LL_ENDL;
            }
            LLFile::mkdir(mAuditRoot);
        }
    }

    // No restore() here: the static owner can be destroyed after viewer cleanup has
    // torn down floaters/settings. FINISH and abortForLogin() restore explicitly.
    ~Runner() = default;

    // The user started a real login mid-test: put everything back and stop.
    void abortForLogin()
    {
        LL_WARNS("UIPerfSelfTest") << "UIPerfSelfTest aborted: login started" << LL_ENDL;
        if (LLFloater* floater = mFloater.get())
        {
            if (mOriginalFloaterRect.getWidth() > 0 && mOriginalFloaterRect.getHeight() > 0)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
        }
        restoreInitialState();
        restore();
        sActive = false;
        sSuppressSettingsSave = false;
        sLayoutAuditActive = false;
    }

    bool tick() { return step(); }

    void captureAfterRender()
    {
        if (!mCaptureRequested || mCapturedImage.notNull()) return;
        LLFloater* floater = mFloater.get();
        if (!floater || !gViewerWindow) return;

        LLRect screen = floater->calcScreenRect();
        const LLVector2& scale = gViewerWindow->getDisplayScale();
        LLRect raw(ll_round((F32)screen.mLeft * scale.mV[VX]),
                   ll_round((F32)screen.mTop * scale.mV[VY]),
                   ll_round((F32)screen.mRight * scale.mV[VX]),
                   ll_round((F32)screen.mBottom * scale.mV[VY]));
        const LLRect window = gViewerWindow->getWindowRectRaw();
        raw.mLeft = llclamp(raw.mLeft, window.mLeft, window.mRight);
        raw.mRight = llclamp(raw.mRight, window.mLeft, window.mRight);
        raw.mBottom = llclamp(raw.mBottom, window.mBottom, window.mTop);
        raw.mTop = llclamp(raw.mTop, window.mBottom, window.mTop);
        if (raw.getWidth() <= 0 || raw.getHeight() <= 0)
        {
            mCaptureRequested = false;
            mCaptureFailed = true;
            return;
        }

        LLPointer<LLImageRaw> image = new LLImageRaw(
            (U16)raw.getWidth(), (U16)raw.getHeight(), 3);
        GLint saved_read_fbo = 0;
        GLint saved_pack_buffer = 0;
        GLint saved_read_buffer = GL_BACK;
        GLint saved_pack_alignment = 4;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &saved_read_fbo);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &saved_pack_buffer);
        glGetIntegerv(GL_READ_BUFFER, &saved_read_buffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &saved_pack_alignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        {
            LLImageDataLock lock(image);
            glReadPixels(raw.mLeft, raw.mBottom, raw.getWidth(), raw.getHeight(),
                         GL_RGB, GL_UNSIGNED_BYTE, image->getData());
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(saved_pack_buffer));
        glPixelStorei(GL_PACK_ALIGNMENT, saved_pack_alignment);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(saved_read_fbo));
        glReadBuffer(static_cast<GLenum>(saved_read_buffer));
        mCapturedImage = image;
        mCaptureRequested = false;
    }

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
        AUDIT_SIZE_PREP,
        AUDIT_SIZE_SETTLE,
        AUDIT_PAGE_SELECT,
        AUDIT_PAGE_SETTLE,
        AUDIT_CAPTURE_SETTLE,
        AUDIT_CAPTURE_WAIT,
        AUDIT_RESTORE,
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

    std::string auditSizeName() const
    {
        return mAuditSizeIndex == 0 ? "default" : "minimum";
    }

    std::string auditPagePath() const
    {
        if (mAuditPageIndex == 0) return "default";
        const size_t tab_index = mAuditPageIndex - 1;
        return tab_index < mTabs.size() ? mTabs[tab_index].mPath : "invalid";
    }

    LLView* auditPageRoot() const
    {
        if (mAuditPageIndex == 0) return mFloater.get();
        const size_t tab_index = mAuditPageIndex - 1;
        return tab_index < mTabs.size() ? mTabs[tab_index].mPanel.get() : nullptr;
    }

    bool selectAuditPage()
    {
        restoreInitialState();
        if (mAuditPageIndex == 0) return mFloater.get() != nullptr;
        const size_t tab_index = mAuditPageIndex - 1;
        return tab_index < mTabs.size() && applyChain(mTabs[tab_index].mChain);
    }

    void addFinding(const std::string& type, const AuditLeaf& a,
                    const AuditLeaf* b, const std::string& detail)
    {
        const std::string key = auditSizeName() + "|" + mTargets[mTargetIndex] +
            "|" + auditPagePath() + "|" + type + "|" + a.mPath + "|" +
            (b ? b->mPath : std::string()) + "|" + detail;
        if (!mFindingKeys.insert(key).second) return;
        LayoutFinding finding;
        finding.mSizePass = auditSizeName();
        finding.mFloater = mTargets[mTargetIndex];
        finding.mPageIndex = (U32)mAuditPageIndex;
        finding.mPath = auditPagePath();
        finding.mFinding = type;
        finding.mWidgetA = a.mPath;
        finding.mRectA = a.mRect;
        if (b)
        {
            finding.mWidgetB = b->mPath;
            finding.mRectB = b->mRect;
        }
        finding.mDetail = detail;
        mFindings.push_back(std::move(finding));
    }

    void auditGeometry()
    {
        LLView* page = auditPageRoot();
        LLFloater* floater = mFloater.get();
        if (!page || !floater) return;
        mAuditedPages.insert(auditSizeName() + " " + mTargets[mTargetIndex] +
                             "/" + auditPagePath());

        std::vector<LLView*> views;
        std::vector<AuditLeaf> leaves;
        collectAuditViews(page, page, views, leaves);
        const LLRect floater_local = floater->getLocalRect();
        for (AuditLeaf& leaf : leaves)
        {
            leaf.mContainingScroller = containingScroller(leaf.mView, floater);
        }

        for (const AuditLeaf& leaf : leaves)
        {
            LLView* parent = leaf.mView->getParent();
            const bool tab_button = dynamic_cast<LLTabContainer*>(parent) != nullptr;
            if (tab_button)
            {
                LLButton* button = dynamic_cast<LLButton*>(leaf.mView);
                if (button && button->getFont() && !button->getText().empty())
                {
                    const S32 required = button->getFont()->getWidth(button->getText()) +
                        button->getLeftHPad() + button->getRightHPad();
                    if (required > button->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("label+padding=%d available=%d", required,
                                     button->getRect().getWidth()));
                    }
                }
                continue;
            }
            if (parent && !isRealCompositeControl(parent))
            {
                const LLRect& rect = leaf.mView->getRect();
                const LLRect bounds = parent->getLocalRect();
                if (rect.mLeft < bounds.mLeft - 1 || rect.mRight > bounds.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr,
                        "outside parent " + parent->getName());
                }
                if (rect.mBottom < bounds.mBottom - 1 || rect.mTop > bounds.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr,
                        "outside parent " + parent->getName());
                }
            }

            LLScrollContainer* scroller = leaf.mContainingScroller;
            if (scroller && scroller->getScrolledView())
            {
                LLRect in_document;
                leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                                  &in_document,
                                                  scroller->getScrolledView());
                const LLRect document = scroller->getScrolledView()->getLocalRect();
                if (in_document.mLeft < document.mLeft - 1 ||
                    in_document.mRight > document.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr, "outside scroll document");
                }
                if (in_document.mBottom < document.mBottom - 1 ||
                    in_document.mTop > document.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr, "outside scroll document");
                }
            }
            else
            {
                LLRect in_floater;
                leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                                  &in_floater, floater);
                if (in_floater.mLeft < floater_local.mLeft - 1 ||
                    in_floater.mRight > floater_local.mRight + 1)
                {
                    addFinding("CLIPPED_H", leaf, nullptr, "outside floater client");
                }
                if (in_floater.mBottom < floater_local.mBottom - 1 ||
                    in_floater.mTop > floater_local.mTop + 1)
                {
                    addFinding("CLIPPED_V", leaf, nullptr, "outside floater client");
                }
            }

            LLRect in_floater;
            leaf.mView->localRectToOtherView(leaf.mView->getLocalRect(),
                                              &in_floater, floater);
            bool horizontal_scroll = false;
            bool vertical_scroll = false;
            containingScrollAxes(leaf.mView, floater,
                                 horizontal_scroll, vertical_scroll);
            if ((!horizontal_scroll && in_floater.mRight > floater_local.mRight + 1) ||
                (!vertical_scroll && in_floater.mBottom < floater_local.mBottom - 1))
            {
                addFinding("OFF_FLOATER", leaf, nullptr,
                    "right/bottom beyond floater without matching scroll axis");
            }

            if (LLTextBox* text = dynamic_cast<LLTextBox*>(leaf.mView))
            {
                if (!text->getWordWrap() && text->getFont() && !text->getText().empty())
                {
                    const S32 width = longestLineWidth(text->getFont(), text->getText());
                    if (width > text->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("text=%d available=%d", width,
                                     text->getRect().getWidth()));
                    }
                }
            }
            if (LLButton* button = dynamic_cast<LLButton*>(leaf.mView);
                button && !dynamic_cast<LLComboBox*>(parent))
            {
                const std::string label = button->getText();
                if (button->getFont() && !label.empty())
                {
                    const S32 required = button->getFont()->getWidth(label) +
                        button->getLeftHPad() + button->getRightHPad();
                    if (required > button->getRect().getWidth() + 1)
                    {
                        addFinding("TEXT_TRUNCATED", leaf, nullptr,
                            llformat("label+padding=%d available=%d", required,
                                     button->getRect().getWidth()));
                    }
                }
            }
            if (dynamic_cast<LLSlider*>(leaf.mView) && leaf.mRect.getWidth() < 80)
            {
                addFinding("TOO_NARROW", leaf, nullptr,
                    llformat("slider track=%d minimum=80", leaf.mRect.getWidth()));
            }
            if (dynamic_cast<LLLineEditor*>(leaf.mView) && leaf.mRect.getWidth() < 40 &&
                !dynamic_cast<LLSliderCtrl*>(parent) &&
                !dynamic_cast<LLSpinCtrl*>(parent) &&
                !dynamic_cast<LLMultiSliderCtrl*>(parent) &&
                !dynamic_cast<LLComboBox*>(parent))
            {
                addFinding("TOO_NARROW", leaf, nullptr,
                    llformat("line editor=%d minimum=40", leaf.mRect.getWidth()));
            }
        }

        for (LLView* view : views)
        {
            if (LLComboBox* combo = dynamic_cast<LLComboBox*>(view))
            {
                if (combo->getRect().getWidth() < 40)
                {
                    AuditLeaf item;
                    item.mView = combo;
                    item.mPath = relativePath(combo, page);
                    combo->localRectToOtherView(combo->getLocalRect(), &item.mRect, page);
                    addFinding("TOO_NARROW", item, nullptr,
                        llformat("combo=%d minimum=40", combo->getRect().getWidth()));
                }
                const std::string label = combo->getSelectedItemLabel();
                const LLFontGL* font = nullptr;
                for (LLView* child : *combo->getChildList())
                {
                    if (child->getFont()) { font = child->getFont(); break; }
                }
                if (font && !label.empty())
                {
                    const S32 required = font->getWidth(label) + 24;
                    if (required > combo->getRect().getWidth() + 1)
                    {
                        AuditLeaf item;
                        item.mView = combo;
                        item.mPath = relativePath(combo, page);
                        combo->localRectToOtherView(combo->getLocalRect(), &item.mRect, page);
                        addFinding("TEXT_TRUNCATED", item, nullptr,
                            llformat("selected text+arrow=%d available=%d", required,
                                     combo->getRect().getWidth()));
                    }
                }
            }
            if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(view))
            {
                if (spinner->getRect().getWidth() < 40)
                {
                    AuditLeaf item;
                    item.mView = spinner;
                    item.mPath = relativePath(spinner, page);
                    spinner->localRectToOtherView(spinner->getLocalRect(), &item.mRect, page);
                    addFinding("TOO_NARROW", item, nullptr,
                        llformat("spinner=%d minimum=40", spinner->getRect().getWidth()));
                }
            }
        }

        std::vector<AuditLeaf> overlap_units;
        overlap_units.reserve(leaves.size());
        for (const AuditLeaf& leaf : leaves)
        {
            if (!dynamic_cast<LLTabContainer*>(leaf.mView->getParent()))
            {
                AuditLeaf unit = leaf;
                unit.mRect = overlapRect(unit);
                overlap_units.push_back(std::move(unit));
            }
        }
        std::vector<LLHandle<LLScrollContainer>> scrollers;
        collectAllScrollers(page, scrollers);
        for (const LLHandle<LLScrollContainer>& handle : scrollers)
        {
            LLScrollContainer* scroller = handle.get();
            if (!scroller || !visibleWithin(scroller, page)) continue;
            AuditLeaf unit;
            unit.mView = scroller;
            unit.mContainingScroller = containingScroller(scroller, floater);
            unit.mPath = relativePath(scroller, page);
            scroller->localRectToOtherView(scroller->getLocalRect(), &unit.mRect, page);
            if (unit.mRect.getWidth() <= 0 || unit.mRect.getHeight() <= 0) continue;
            for (LLView* cursor = scroller; cursor && cursor != page;
                 cursor = cursor->getParent())
            {
                if (!cursor->getName().empty()) unit.mLineage.insert(cursor->getName());
            }
            overlap_units.push_back(std::move(unit));
        }

        for (size_t i = 0; i < overlap_units.size(); ++i)
        {
            for (size_t j = i + 1; j < overlap_units.size(); ++j)
            {
                const AuditLeaf& a = overlap_units[i];
                const AuditLeaf& b = overlap_units[j];
                const S32 overlap_x = llmin(a.mRect.mRight, b.mRect.mRight) -
                                      llmax(a.mRect.mLeft, b.mRect.mLeft);
                const S32 overlap_y = llmin(a.mRect.mTop, b.mRect.mTop) -
                                      llmax(a.mRect.mBottom, b.mRect.mBottom);
                if (overlap_x <= 2 || overlap_y <= 2) continue;
                if (a.mContainingScroller != b.mContainingScroller) continue;
                if (a.mView->hasAncestor(b.mView) || b.mView->hasAncestor(a.mView)) continue;
                LLView* common = nearestCommonAncestor(a.mView, b.mView, page);
                if ((common && isRealCompositeControl(common)) ||
                    mOverlapPolicy.allows(a, b))
                {
                    continue;
                }
                const bool a_icon = dynamic_cast<LLIconCtrl*>(a.mView) != nullptr;
                const bool b_icon = dynamic_cast<LLIconCtrl*>(b.mView) != nullptr;
                const bool a_contains_b = a.mRect.mLeft <= b.mRect.mLeft &&
                    a.mRect.mRight >= b.mRect.mRight &&
                    a.mRect.mBottom <= b.mRect.mBottom && a.mRect.mTop >= b.mRect.mTop;
                const bool b_contains_a = b.mRect.mLeft <= a.mRect.mLeft &&
                    b.mRect.mRight >= a.mRect.mRight &&
                    b.mRect.mBottom <= a.mRect.mBottom && b.mRect.mTop >= a.mRect.mTop;
                if ((a_icon || b_icon) && (a_contains_b || b_contains_a)) continue;
                addFinding("OVERLAP", a, &b,
                    llformat("intersection=%dx%d", overlap_x, overlap_y));
            }
        }
    }

    void prepareCapturePositions()
    {
        mCaptureScroller = LLHandle<LLScrollContainer>();
        mCapturePositions.clear();
        mCapturePositions.push_back(0);
        LLView* page = auditPageRoot();
        if (!page) return;
        std::vector<LLHandle<LLScrollContainer>> scrollers;
        collectAllScrollers(page, scrollers);
        S32 best_overflow = 0;
        S32 best_viewport = 0;
        for (const LLHandle<LLScrollContainer>& handle : scrollers)
        {
            LLScrollContainer* scroller = handle.get();
            if (!scroller || !visibleWithin(scroller, page) || !scroller->getScrolledView()) continue;
            const S32 viewport = scroller->getVisibleContentRect().getHeight();
            const S32 overflow = scroller->getScrolledView()->getLocalRect().getHeight() - viewport;
            if (overflow > best_overflow && viewport > 0)
            {
                best_overflow = overflow;
                best_viewport = viewport;
                mCaptureScroller = handle;
            }
        }
        if (best_overflow <= 0) return;
        mCapturePositions.clear();
        for (S32 pos = 0; pos < best_overflow && mCapturePositions.size() < 5;
             pos += best_viewport)
        {
            mCapturePositions.push_back(pos);
        }
        if (mCapturePositions.empty() || mCapturePositions.back() != best_overflow)
        {
            mCapturePositions.push_back(best_overflow);
        }
    }

    void requestCapture()
    {
        const std::string size_dir = mAuditRoot + gDirUtilp->getDirDelimiter() + auditSizeName();
        const std::string floater_dir = size_dir + gDirUtilp->getDirDelimiter() +
            safeComponent(mTargets[mTargetIndex]);
        LLFile::mkdir(size_dir);
        LLFile::mkdir(floater_dir);
        mCaptureFilename = floater_dir + gDirUtilp->getDirDelimiter() +
            llformat("%03u_", (U32)mAuditPageIndex) + safeComponent(auditPagePath()) + "_" +
            llformat("%u", (U32)mCapturePositionIndex) + ".png";
        mCaptureFailed = false;
        mCaptureWaitFrames = 0;
        mCaptureRequested = true;
    }

    void saveCapturedImage()
    {
        if (mCapturedImage.isNull()) return;
        LLPointer<LLImagePNG> png = new LLImagePNG;
        if (!png->encode(mCapturedImage, 0.f) || !png->save(mCaptureFilename))
        {
            LL_WARNS("UILayoutAudit") << "Unable to write " << mCaptureFilename << LL_ENDL;
        }
        mCapturedImage = nullptr;
    }

    void beginAuditOrClose()
    {
        if (mLayout && (!mAB || !mCullOn))
        {
            mAuditSizeIndex = 0;
            mAuditPageIndex = 0;
            mPhase = AUDIT_SIZE_PREP;
        }
        else
        {
            mPhase = CLOSE;
        }
    }

    void closeCurrentTarget()
    {
        restoreInitialState();
        if (LLFloater* floater = mFloater.get())
        {
            if (mOriginalFloaterRect.getWidth() > 0 && mOriginalFloaterRect.getHeight() > 0)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            LLFloaterReg::hideInstance(mTargets[mTargetIndex]);
        }
        mFloater = LLHandle<LLFloater>();
        mCaptureRequested = false;
        mCaptureFailed = false;
        mCapturedImage = nullptr;
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
            mOriginalFloaterRect = LLRect();
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
                LLFloater* floater = mFloater.get();
                if (!floater)
                {
                    abortCurrentTarget("floater died before settled rect capture");
                    break;
                }
                mOriginalFloaterRect = floater->getRect();
                addRow(mTargets[mTargetIndex], "default", "steady");
                if (!mDefaultScrollers.empty())
                {
                    mInDefaultScroll = true;
                    mScrollIndex = 0;
                    mPhase = SCROLL_BOTTOM;
                }
                else
                {
                    if (mTabs.empty()) beginAuditOrClose();
                    else mPhase = TAB_SELECT;
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
                if (mScrollIndex < target.mScrollEnd)
                {
                    mPhase = SCROLL_BOTTOM;
                }
                else if (++mTabIndex < mTabs.size())
                {
                    mPhase = TAB_SELECT;
                }
                else
                {
                    beginAuditOrClose();
                }
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
                        if (mTabs.empty()) beginAuditOrClose();
                        else mPhase = TAB_SELECT;
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
                    if (mTabIndex < mTabs.size()) mPhase = TAB_SELECT;
                    else beginAuditOrClose();
                }
            }
            break;
        case AUDIT_SIZE_PREP:
        {
            FrameSample ignored;
            takeFrame(ignored);
            LLFloater* floater = mFloater.get();
            if (!floater)
            {
                abortCurrentTarget("floater died during layout audit");
                break;
            }
            restoreInitialState();
            if (mAuditSizeIndex == 0)
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            else
            {
                const S32 min_width = floater->getMinWidth();
                const S32 min_height = floater->getMinHeight();
                if (!floater->isResizable())
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: not resizable "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    mPhase = AUDIT_RESTORE;
                    break;
                }
                if (min_width <= 0 || min_height <= 0)
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: invalid minimum "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    mPhase = AUDIT_RESTORE;
                    break;
                }
                const S32 width = llmin(min_width, mOriginalFloaterRect.getWidth());
                const S32 height = llmin(min_height, mOriginalFloaterRect.getHeight());
                if (width == mOriginalFloaterRect.getWidth() &&
                    height == mOriginalFloaterRect.getHeight())
                {
                    LL_INFOS("UILayoutAudit") << "UILayoutAudit minimum skipped: matches original "
                        << mTargets[mTargetIndex] << LL_ENDL;
                    mPhase = AUDIT_RESTORE;
                    break;
                }
                floater->reshape(width, height, false);
            }
            mAuditPageIndex = 0;
            mAuditSettleFrames = 0;
            mAuditSettleTimer.reset();
            mPhase = AUDIT_SIZE_SETTLE;
            break;
        }
        case AUDIT_SIZE_SETTLE:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mAuditSettleFrames >= 5 &&
                mAuditSettleTimer.getElapsedTimeF32() >= 0.3f)
            {
                mPhase = AUDIT_PAGE_SELECT;
            }
            break;
        }
        case AUDIT_PAGE_SELECT:
        {
            FrameSample ignored;
            takeFrame(ignored);
            const bool prior_flush = LLError::getAlwaysFlush();
            LLError::setAlwaysFlush(true);
            LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest begin "
                << mTargets[mTargetIndex] << "/audit-" << auditSizeName()
                << "/" << auditPagePath() << LL_ENDL;
            LLError::setAlwaysFlush(prior_flush);
            if (!selectAuditPage() || !auditPageRoot())
            {
                abortCurrentTarget("layout audit page died: " + auditPagePath());
                break;
            }
            std::vector<LLHandle<LLScrollContainer>> scrollers;
            collectAllScrollers(auditPageRoot(), scrollers);
            for (const LLHandle<LLScrollContainer>& handle : scrollers)
            {
                if (LLScrollContainer* scroller = handle.get()) scroller->goToTop();
            }
            mAuditSettleFrames = 0;
            mAuditSettleTimer.reset();
            mPhase = AUDIT_PAGE_SETTLE;
            break;
        }
        case AUDIT_PAGE_SETTLE:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && ++mAuditSettleFrames >= 5 &&
                mAuditSettleTimer.getElapsedTimeF32() >= 0.3f)
            {
                auditGeometry();
                prepareCapturePositions();
                mCapturePositionIndex = 0;
                mAuditSettleFrames = 0;
                mPhase = AUDIT_CAPTURE_SETTLE;
            }
            break;
        }
        case AUDIT_CAPTURE_SETTLE:
        {
            FrameSample ignored;
            if (!takeFrame(ignored)) break;
            if (++mAuditSettleFrames < 2) break;
            requestCapture();
            mPhase = AUDIT_CAPTURE_WAIT;
            break;
        }
        case AUDIT_CAPTURE_WAIT:
        {
            FrameSample ignored;
            if (takeFrame(ignored) && mCaptureRequested && ++mCaptureWaitFrames >= 30)
            {
                mCaptureFailed = true;
                mCaptureRequested = false;
                LL_WARNS("UILayoutAudit") << "UILayoutAudit capture timeout "
                    << mTargets[mTargetIndex] << "/" << auditPagePath() << LL_ENDL;
            }
            if (mCaptureRequested) break;
            if (!mCaptureFailed) saveCapturedImage();
            ++mCapturePositionIndex;
            if (mCapturePositionIndex < mCapturePositions.size())
            {
                if (LLScrollContainer* scroller = mCaptureScroller.get())
                {
                    scroller->setDocPosVertical(mCapturePositions[mCapturePositionIndex]);
                }
                mAuditSettleFrames = 0;
                mPhase = AUDIT_CAPTURE_SETTLE;
                break;
            }
            ++mAuditPageIndex;
            if (mAuditPageIndex <= mTabs.size())
            {
                mPhase = AUDIT_PAGE_SELECT;
            }
            else if (++mAuditSizeIndex < 2)
            {
                mPhase = AUDIT_SIZE_PREP;
            }
            else
            {
                mPhase = AUDIT_RESTORE;
            }
            break;
        }
        case AUDIT_RESTORE:
        {
            FrameSample ignored;
            takeFrame(ignored);
            restoreInitialState();
            if (LLFloater* floater = mFloater.get())
            {
                floater->setShape(mOriginalFloaterRect, false);
            }
            mPhase = CLOSE;
            break;
        }
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
            sLayoutAuditActive = false;
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
        if (mLayout)
        {
            writeLayoutResults();
        }
        LL_INFOS("UIPerfSelfTest") << "UIPerfSelfTest summary complete" << LL_ENDL;
    }

    void writeLayoutResults()
    {
        const std::string path = gDirUtilp->getExpandedFilename(
            LL_PATH_LOGS, "ui_layout_audit.csv");
        llofstream out(path.c_str(), std::ios::out | std::ios::trunc);
        out << "size_pass,floater,page_index,tab_path,finding,widget_a,widget_b,rect_a,rect_b,detail\n";
        for (const LayoutFinding& finding : mFindings)
        {
            out << csv(finding.mSizePass) << ',' << csv(finding.mFloater) << ','
                << finding.mPageIndex << ',' << csv(finding.mPath) << ','
                << csv(finding.mFinding) << ','
                << csv(finding.mWidgetA) << ',' << csv(finding.mWidgetB) << ','
                << csv(rectString(finding.mRectA)) << ','
                << csv(finding.mWidgetB.empty() ? std::string() : rectString(finding.mRectB))
                << ',' << csv(finding.mDetail) << '\n';
        }
        out.flush();

        std::map<std::string, U32> by_page;
        std::map<std::string, U32> by_type;
        for (const LayoutFinding& finding : mFindings)
        {
            ++by_page[finding.mSizePass + " " + finding.mFloater + "/" + finding.mPath];
            ++by_type[finding.mFinding];
        }
        for (const std::string& page : mAuditedPages)
        {
            by_page.try_emplace(page, 0);
        }
        LL_INFOS("UILayoutAudit") << "UILayoutAudit summary begin: "
            << mFindings.size() << " findings; CSV=" << path
            << " screenshots=" << mAuditRoot << LL_ENDL;
        for (const auto& entry : by_type)
        {
            LL_INFOS("UILayoutAudit") << "type " << entry.first << "=" << entry.second << LL_ENDL;
        }
        for (const auto& entry : by_page)
        {
            LL_INFOS("UILayoutAudit") << "page " << entry.first << "=" << entry.second << LL_ENDL;
        }
        LL_INFOS("UILayoutAudit") << "UILayoutAudit summary complete" << LL_ENDL;
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
    bool mLayout;
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
    LLRect mOriginalFloaterRect;
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
    OverlapPolicy mOverlapPolicy;
    std::vector<LayoutFinding> mFindings;
    std::set<std::string> mFindingKeys;
    std::set<std::string> mAuditedPages;
    std::string mAuditRoot;
    U32 mAuditSizeIndex = 0;
    size_t mAuditPageIndex = 0;
    U32 mAuditSettleFrames = 0;
    LLFrameTimer mAuditSettleTimer;
    LLHandle<LLScrollContainer> mCaptureScroller;
    std::vector<S32> mCapturePositions;
    size_t mCapturePositionIndex = 0;
    bool mCaptureRequested = false;
    bool mCaptureFailed = false;
    U32 mCaptureWaitFrames = 0;
    std::string mCaptureFilename;
    LLPointer<LLImageRaw> mCapturedImage;
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
    static LLCachedControl<bool> layout(gSavedSettings, "UILayoutAudit", false);
    sActive = true;
    sSuppressSettingsSave = true;
    sLayoutAuditActive = layout;
    sRunner = std::make_unique<Runner>(request(), frames, ab, layout);
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

void ALUIPerfSelfTest::captureAfterRender()
{
    if (sRunner)
    {
        sRunner->captureAfterRender();
    }
}

bool ALUIPerfSelfTest::layoutAuditActive()
{
    return sLayoutAuditActive;
}

bool ALUIPerfSelfTest::suppressSettingsSave()
{
    return sSuppressSettingsSave;
}

bool ALUIPerfSelfTest::isActive()
{
    return sActive;
}
