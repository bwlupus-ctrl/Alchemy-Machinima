#!/usr/bin/env python3
"""Director background-only fade contract, native alpha checks and planted negatives.

All kit rectangle/image paint sites are classified: background fills must use the
accessor; the exact reviewed control/icon/selection paints must not. New paint
paths fail closed until classified. Standard LLUI backgrounds inherit the same
owner multiplier; ordinary transparency, text and control painting stay separate.
"""
from __future__ import annotations

import argparse
import copy
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NV = 'indra/newview/'
UI = 'indra/llui/'
HELP = ("How solid the Director's backgrounds are. 100% = solid; lower lets the world "
        "show through. Text and controls stay solid.")
ACCESSOR = ' % ALKitStyle::backgroundAlpha(this)'


def compact(text):
    return re.sub(r'\s+', '', text)


# Only control faces, icons, hover/selection and focus/flash outlines stay solid.
# This is a paint classification, not a finding/baseline allowance.
SOLID_PAINT = {
    'alkitbar.cpp': (
        'gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::ARC_TRACK).get(), true)',
        'gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::INPUT_BORDER).get(), false)',
    ),
    'alkitpage.cpp': (
        'search->draw(5, (getRect().getHeight() - 16) / 2, 16, 16, ALKitStyle::color(ALKitStyle::MUTED).get())',
    ),
    'alkitrow.cpp': (
        'gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::ROW_HOVER).get(), true)',
        'gl_rect_2d(getLocalRect(), c, false)',
        'gl_rect_2d(segment, ALKitStyle::color(index == selected ? ALKitStyle::ACTIVE_FILL : ALKitStyle::INPUT_BG).get(), true)',
        'gl_rect_2d(segment, ALKitStyle::color(mControl->hasFocus() && index == selected ? ALKitStyle::ACCENT : ALKitStyle::INPUT_BORDER).get(), false)',
    ),
    'alkitshell.cpp': (
        'gl_rect_2d(rect, color, false)',
        'gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::ROW_HOVER).get(), true)',
        'gl_rect_2d(rect, ALKitStyle::color(ALKitStyle::CONTROL_BG).get(), true)',
        'gl_rect_2d(rect.mLeft, rect.mBottom, rect.mRight, rect.mBottom + 2, ALKitStyle::color(ALKitStyle::ACCENT).get(), true)',
        'gl_rect_2d(expanded, ring, false)',
    ),
    'alkitswitch.cpp': (
        'gl_rect_2d(ll_round(left_x), track_rect.mTop, ll_round(right_x), track_rect.mBottom, track, true)',
    ),
}


def block(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def paints(text):
    # Comments cannot masquerade as working draw calls or hook wiring.
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
    for match in re.finditer(r'(?:gl_rect_2d(?:_offset_local)?|gl_drop_shadow|\w+->draw)\s*\(', text):
        depth, end = 1, match.end()
        while depth:
            depth += (text[end] == '(') - (text[end] == ')')
            end += 1
        yield text[match.start():end]


def sources(root=ROOT):
    paths = list((root / NV).glob('alkit*.cpp'))
    paths += list((root / UI).glob('*.cpp'))
    paths += [root / (UI + p) for p in ('llview.h', 'llfloater.h', 'lluictrl.h', 'lluictrl.cpp',
              'llpanel.cpp', 'llfloater.cpp', 'llscrollcontainer.cpp',
              'llscrolllistctrl.cpp', 'llmenugl.cpp', 'lltextbase.cpp')]
    paths += [root / (NV + p) for p in ('llfloaterdirector.cpp', 'llfloaterdirector.h',
              'app_settings/settings.xml', 'skins/default/xui/en/director_kit_index.xml',
              'skins/default/xui/en/panel_dpage_settings_director_settings.xml')]
    return {p.relative_to(root).as_posix(): p.read_text(encoding='utf-8') for p in paths}


def check(src):
    errors = []

    def require(path, token):
        if compact(token) not in compact(src[path]):
            errors.append(f'{path}: missing background-only contract: {token}')

    for path, text in src.items():
        name = Path(path).name
        if name.startswith('alkit') and name.endswith('.cpp') and name != 'alkitstyle.cpp':
            solid = {compact(p) for p in SOLID_PAINT.get(name, ())}
            remaining = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)
            for paint in paints(text):
                plain = compact(paint.replace(ACCESSOR, ''))
                faded = 'backgroundAlpha' in paint
                if plain in solid:
                    if faded:
                        errors.append(f'{name}: control/icon/highlight alpha multiplied: {paint}')
                elif not faded:
                    errors.append(f'{name}: unclassified/background paint bypasses accessor: {paint}')
                elif re.search(r'ALKitStyle::(?:TEXT\w*|LABEL|MUTED|FAINT|ICON\w*)\b', paint):
                    errors.append(f'{name}: foreground paint faded: {paint}')
                remaining = remaining.replace(paint, '', 1)
            if 'backgroundAlpha' in remaining or 'getBackgroundOpacity' in remaining:
                errors.append(f'{name}: background multiplier outside classified background paint')

    require(UI + 'llview.h', 'mutable LLHandle<LLView> mBackgroundOpacityOwner;')
    require(UI + 'llview.h', 'virtual bool ownsBackgroundOpacity() const { return false; }')
    require(UI + 'llview.h', 'virtual F32 getOwnBackgroundOpacity() const { return 1.f; }')
    require(UI + 'llview.cpp', 'if (!mBackgroundOpacityResolved || mBackgroundOpacityGeneration != sTreeGeneration)')
    require(UI + 'llview.cpp', 'mBackgroundOpacityOwner = owner ? owner->getHandle() : LLHandle<LLView>();')
    require(UI + 'llview.cpp', 'return owner ? owner->getOwnBackgroundOpacity() : 1.f;')
    require(UI + 'llview.h', 'mutable U64 mBackgroundOpacityGeneration = 0;')
    require(UI + 'llview.h', 'static U64 sTreeGeneration;')
    require(UI + 'llview.cpp', 'U64 LLView::sTreeGeneration = 0;')
    require(UI + 'llfloater.h', 'U64 mAppliedTreeGeneration { 0 };')
    require(UI + 'llview.cpp', 'if (owner != this && owner->mBackgroundOpacityResolved && owner->mBackgroundOpacityGeneration == sTreeGeneration)')
    require(UI + 'llview.cpp', 'mBackgroundOpacityOwner = owner->mBackgroundOpacityOwner;')
    require(UI + 'llview.cpp', 'ancestor->mBackgroundOpacityOwner = mBackgroundOpacityOwner;')
    require(UI + 'llview.cpp', 'ancestor->mBackgroundOpacityGeneration = sTreeGeneration;')
    require(UI + 'llview.cpp', 'ancestor->mBackgroundOpacityResolved = true;')
    require(UI + 'llview.cpp', 'ancestor = ancestor->mParentView;')
    for signature in ('bool LLView::addChild(', 'void LLView::removeChild(',
                      'void LLView::deleteAllChildren()', 'LLView::~LLView()'):
        if '++sTreeGeneration;' not in block(src[UI + 'llview.cpp'], signature):
            errors.append('owner cache invalidation missing: ' + signature)
    require(UI + 'lluictrl.h', 'F32 getBackgroundTransparency() { return getCurrentTransparency() * getBackgroundOpacity(); }')
    require(NV + 'alkitstyle.cpp', 'return view ? view->getBackgroundOpacity() : 1.f;')
    require(NV + 'llfloaterdirector.cpp', 'return mIsV2 ? mBackgroundOpacity : 1.f;')
    require(NV + 'llfloaterdirector.h', 'F32 mBackgroundOpacity = 1.f;')
    require(NV + 'llfloaterdirector.h', 'bool ownsBackgroundOpacity() const override { return true; }')
    underlay = block(src[NV + 'llfloaterdirector.cpp'], 'void LLFloaterDirector::drawBackgroundUnderlay()')
    for token in ('if (!mIsV2) return;',
                  'LLColor4 backing = ALKitStyle::color(ALKitStyle::WINDOW_BG).get();',
                  'backing.mV[VALPHA] = getBackgroundOpacity();',
                  'gl_rect_2d(getLocalRect(), backing, true);'):
        if compact(token) not in compact(underlay):
            errors.append('Director background underlay contract missing: ' + token)
    floater_draw = block(src[UI + 'llfloater.cpp'], 'void LLFloater::draw()')
    if ('drawBackgroundUnderlay();' not in floater_draw or
            floater_draw.index('drawBackgroundUnderlay();') > floater_draw.index('drawShadow(this);')):
        errors.append('backing must be inside floater background pass before shadow/chrome')
    require(UI + 'llfloater.h', 'virtual void drawBackgroundUnderlay() {}')
    director = src[NV + 'llfloaterdirector.cpp']
    draw = block(director, 'void LLFloaterDirector::draw()')
    require(NV + 'llfloaterdirector.cpp', 'static LLCachedControl<F32> background_opacity(gSavedSettings, "DirectorBackgroundOpacity", 1.f);')
    if ('mBackgroundOpacity = llclamp(background_opacity(), 0.f, 1.f);' not in draw
            or draw.count('background_opacity()') != 1
            or draw.index('mBackgroundOpacity =') > draw.index('LLFloater::draw();')):
        errors.append('Director: live clamped cache must be read once before child drawing')
    if re.search(r'gl_rect_2d|WINDOW_BG|backing', draw):
        errors.append('Director: redundant backing quad reintroduced')
    transparency = block(src[UI + 'lluictrl.cpp'], 'F32 LLUICtrl::getCurrentTransparency()')
    if 'Background' in transparency or 'background' in transparency:
        errors.append('ordinary text/control transparency must not use background opacity')
    for name in ('llpanel', 'llscrollcontainer', 'llscrolllistctrl'):
        require(UI + name + '.cpp', 'F32 alpha = getBackgroundTransparency();')
    # Compare entire production paint calls, including every argument/operator.
    # The compositing model below is separate from this production source contract.
    expected_paints = (
        'image->draw(getLocalRect(), overlay_color % background_alpha)',
        'gl_rect_2d(left, top, right, bottom, color % background_alpha)',
        'gl_rect_2d_offset_local(0, r.getHeight(), r.getWidth(), '
        'r.getHeight() - font->getLineHeight() - 1, titlebar_focus_color % background_alpha, 0, true)',
    )
    shadow = block(src[UI + 'llfloater.cpp'], 'void    LLFloater::drawShadow(')
    for label, body, expected in (
        ('floater background/titlebar', floater_draw, expected_paints),
        ('shadow', shadow, ('gl_drop_shadow(left, top, right, bottom, '
            'shadow_color % getBackgroundTransparency(), ll_round(shadow_offset))',)),
        ('backing', underlay, ('gl_rect_2d(getLocalRect(), backing, true)',)),
    ):
        if [compact(p) for p in paints(body)] != [compact(p) for p in expected]:
            errors.append(label + ': complete production paint expressions changed')
    draw_items = block(src[UI + 'llscrolllistctrl.cpp'], 'void LLScrollListCtrl::drawItems()')
    for token in ('// Local alpha excludes background opacity: stripes/disabled rows are pre-multiplied once.',
                  'F32 alpha = getDrawContext().mAlpha;'):
        if token not in draw_items:
            errors.append('drawItems: local alpha must exclude background multiplier: ' + token)
    for token in ('hover_color = mBgStripeColor.get() % getBackgroundOpacity();',
                  'hover_color = mBgReadOnlyColor.get() % getBackgroundOpacity();',
                  'item->draw(item_rect, fg_color % alpha, hover_color% alpha, select_color% alpha, highlight_color % alpha, mColumnPadding, getSelectedBarGutter());'):
        require(UI + 'llscrolllistctrl.cpp', token)
    require(UI + 'llmenugl.cpp', 'mBackgroundColor.get() % getBackgroundOpacity()')
    require(UI + 'lltextbase.cpp', 'F32 alpha = mReadOnly ? getBackgroundTransparency() : getCurrentTransparency();')
    allowed = {
        'llview.cpp': (block(src[UI + 'llview.cpp'], 'F32 LLView::getBackgroundOpacity() const'),),
        'llpanel.cpp': ('F32 alpha = getBackgroundTransparency();',),
        'llscrollcontainer.cpp': ('F32 alpha = getBackgroundTransparency();',),
        'llscrolllistctrl.cpp': ('F32 alpha = getBackgroundTransparency();',
            'hover_color = mBgStripeColor.get() % getBackgroundOpacity();',
            'hover_color = mBgReadOnlyColor.get() % getBackgroundOpacity();'),
        'lltextbase.cpp': ('F32 alpha = mReadOnly ? getBackgroundTransparency() : getCurrentTransparency();',),
        'llfloater.cpp': ('const F32 background_alpha = getBackgroundTransparency();',
            'shadow_color % getBackgroundTransparency()'),
        'llmenugl.cpp': ('mBackgroundColor.get() % getBackgroundOpacity()',),
    }
    for path, text in src.items():
        if path.startswith(UI) and path.endswith('.cpp'):
            remaining = compact(re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S))
            for token in allowed.get(Path(path).name, ()):
                remaining = remaining.replace(compact(re.sub(r'/\*.*?\*/|//[^\n]*', '', token, flags=re.S)), '', 1)
            if re.search(r'getBackground(?:Opacity|Transparency)|backgroundAlpha|getOwnBackgroundOpacity', remaining):
                errors.append(path + ': background multiplier escaped into shared LLUI paint')
    settings = ET.fromstring(src[NV + 'app_settings/settings.xml'])
    nodes = list(settings.find('map'))
    setting = nodes[next(i for i, n in enumerate(nodes) if n.tag == 'key' and n.text == 'DirectorBackgroundOpacity') + 1]
    values = list(setting)
    fields = {values[i].text: values[i + 1].text for i in range(0, len(values), 2)}
    if fields.get('Value') != '1.0' or fields.get('Type') != 'F32' or fields.get('Persist') != '1':
        errors.append('DirectorBackgroundOpacity must remain persistent F32 default 1.0')
    for name in ('director_kit_index.xml', 'panel_dpage_settings_director_settings.xml'):
        if HELP not in src[NV + 'skins/default/xui/en/' + name]:
            errors.append(name + ': stale background opacity help')
    return errors


def native_check(src, negative=False):
    from native_check_tools import check_directory, compile_run
    # Actual owner resolution, add/remove paths, background scalar, underlay paint
    # and cache refresh run against a small tree; no mock foreground claim is made.
    inherited = block(src[UI + 'llview.cpp'], 'F32 LLView::getBackgroundOpacity() const')
    inherited = inherited.replace('owner = owner->mParentView;', '++probes; owner = owner->mParentView;')
    inherited = inherited.replace('ancestor = ancestor->mParentView;', '++probes; ancestor = ancestor->mParentView;')
    attach = block(src[UI + 'llview.cpp'], 'bool LLView::addChild(')
    detach = block(src[UI + 'llview.cpp'], 'void LLView::removeChild(')
    ctrl = block(src[UI + 'lluictrl.h'], 'F32 getBackgroundTransparency()')
    director = block(src[NV + 'llfloaterdirector.cpp'], 'F32 LLFloaterDirector::getOwnBackgroundOpacity() const')
    underlay = block(src[NV + 'llfloaterdirector.cpp'], 'void LLFloaterDirector::drawBackgroundUnderlay()')
    accessor = block(src[NV + 'alkitstyle.cpp'], 'F32 backgroundAlpha(const LLView* view)')
    refresh = re.search(r'static LLCachedControl<F32> background_opacity[^;]*;\s*mBackgroundOpacity = [^;]*;',
                        src[NV + 'llfloaterdirector.cpp']).group()
    owns = block(src[NV + 'llfloaterdirector.h'], 'bool ownsBackgroundOpacity() const')
    program = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <string>
using F32 = float; using U64 = unsigned long long; using U32 = unsigned; using S32 = int;
#define LL_ERRS() std::cerr
#define LL_WARNS() std::cerr
#define LL_ENDL std::endl
#define llassert(x) assert(x)
int gSavedSettings = 0;
F32 setting = 1.f; int reads = 0, probes = 0;
template<class T> struct LLCachedControl {
    LLCachedControl(int, const char*, T) {}
    T operator()() const { ++reads; return setting; }
};
F32 llclamp(F32 v, F32 a, F32 b) { return std::clamp(v,a,b); }
// Tombstone semantics match LLHandle: handle copies do not extend owner lifetime.
template<class T> struct LLHandle {
    std::shared_ptr<T*> tombstone = std::make_shared<T*>(nullptr);
    T* get() const { return *tombstone; }
};
struct LLView {
    LLView* mParentView = nullptr;
    std::list<LLView*> mChildList;
    using child_tab_order_t = std::map<const LLView*, S32>;
    using tab_order_pair_t = std::pair<LLView*, S32>;
    child_tab_order_t mTabOrder; int mLastTabGroup = 0; bool mInDraw = false;
    static inline U64 sTreeGeneration = 0;
    mutable LLHandle<LLView> mBackgroundOpacityOwner, self;
    mutable U64 mBackgroundOpacityGeneration = 0;
    mutable bool mBackgroundOpacityResolved = false;
    virtual ~LLView() { *self.tombstone = nullptr; }
    LLHandle<LLView> getHandle() const { *self.tombstone = const_cast<LLView*>(this); return self; }
    virtual bool ownsBackgroundOpacity() const { return false; }
    virtual F32 getOwnBackgroundOpacity() const { return 1.f; }
    F32 getBackgroundOpacity() const;
    bool addChild(LLView*, S32 tab_group = 0);
    void removeChild(LLView*);
    bool getVisible() const { return true; }
    void updateBoundingRect() {}
    std::string getName() const { return "fixture"; }
};
struct LLUICtrl : LLView { F32 current = .95f;
    virtual F32 getCurrentTransparency() { return current; }
''' + ctrl + r'''
};
struct LLFloater : LLUICtrl {};
struct LLColor4 { F32 mV[4] = {.12f,.16f,.22f,.37f}; LLColor4 get() const { return *this; } };
constexpr int VALPHA = 3;
namespace ALKitStyle { enum { WINDOW_BG }; LLColor4 color(int) { return {}; } }
F32 painted_backing = 0.f;
void gl_rect_2d(int, LLColor4 c, bool) { painted_backing = c.mV[VALPHA]; }
struct LLFloaterDirector : LLFloater {
    bool mIsV2 = true; F32 mBackgroundOpacity = 1.f;
    ''' + owns + r'''
    F32 getOwnBackgroundOpacity() const override;
    void drawBackgroundUnderlay(); int getLocalRect() const { return 0; }
    void drawRefresh() { ''' + refresh + r''' }
};
''' + inherited + '\n' + attach + '\n' + detach + '\n' + director + '\n' + underlay + '\n' + accessor + r'''
// Straight-alpha source-over model. Samples represent the titlebar, texture's
// rounded and transparent edges, gaps, card interiors, and the external shadow.
struct Pixel { std::array<F32,3> rgb; F32 coverage; };
Pixel composite(const std::array<F32,5>& a) {
    Pixel p{{.67f,.31f,.49f},0.f};
    const std::array<std::array<F32,3>,5> colors{{{.12f,.16f,.22f}, {0,0,0},
        {.24f,.25f,.28f}, {.4f,.5f,.7f}, {.08f,.1f,.14f}}};
    for (int i=0;i<5;++i) {
        for(int c=0;c<3;++c) p.rgb[c]=colors[i][c]*a[i]+p.rgb[c]*(1.f-a[i]);
        p.coverage=a[i]+p.coverage*(1.f-a[i]);
    }
    return p;
}
int main() {
    LLFloaterDirector d, legacy; legacy.mIsV2=false;
    LLFloater other, menu_holder;
    LLView shell; d.addChild(&shell);
    LLUICtrl panel, popover, lazy, independent, legacy_panel, global_popup;
    shell.addChild(&panel); shell.addChild(&popover); popover.addChild(&lazy);
    other.addChild(&independent); legacy.addChild(&legacy_panel);
    menu_holder.addChild(&global_popup); // Global menus never inherit the launcher/host.
    int failures=0;
    auto expect=[&](bool ok) { if(!ok) ++failures; };
    for (F32 value : {1.f,.5f,0.f,-1.f,2.f}) {
        setting=value; reads=0; d.drawRefresh(); legacy.drawRefresh();
        F32 multiplier=std::clamp(value,0.f,1.f);
        for (auto view : {&panel,&popover,&lazy}) {
            expect(std::abs(view->getBackgroundTransparency()-.95f*multiplier)<.00001f);
            expect(backgroundAlpha(view)==multiplier);
        }
        expect(independent.getBackgroundTransparency()==.95f);
        expect(global_popup.getBackgroundOpacity()==1.f);
        expect(legacy_panel.getBackgroundTransparency()==.95f);
        expect(backgroundAlpha(nullptr)==1.f);
        expect(reads==2);
    }
    probes=0;
    for(int i=0;i<1000;++i) { lazy.getBackgroundOpacity(); independent.getBackgroundOpacity(); }
    expect(probes==0); // Warm non-Director and Director lookups are constant time.
    setting=.25f; d.drawRefresh();
    expect(lazy.getBackgroundOpacity()==.25f); // Read live scalar without invalidation.
    other.addChild(&shell); // Reparent an ancestor with already cached descendants.
    expect(panel.getBackgroundOpacity()==1.f && lazy.getBackgroundOpacity()==1.f);
    d.addChild(&shell);
    expect(lazy.getBackgroundOpacity()==.25f);
    d.removeChild(&shell); // Removing without subsequent add must also invalidate.
    expect(lazy.getBackgroundOpacity()==1.f);
    d.addChild(&shell);
    expect(lazy.getBackgroundOpacity()==.25f);
    auto dead_owner = new LLFloaterDirector;
    dead_owner->addChild(&panel); panel.getBackgroundOpacity();
    // Hold the cached generation steady to exercise the weak handle independently.
    delete dead_owner;
    expect(panel.getBackgroundOpacity()==1.f);
    panel.mParentView=nullptr;
    // Parent-before-child paints after unrelated mutations coalesce to one lookup
    // generation. Exercise both owned and resolved-null ancestor cache reuse.
    constexpr int depth=256, frames=24, mutations=37;
    std::array<LLView,depth> owned_chain, plain_chain;
    LLView churn_root, churn_child;
    d.addChild(&owned_chain[0]); other.addChild(&plain_chain[0]);
    for(int i=1;i<depth;++i) {
        owned_chain[i-1].addChild(&owned_chain[i]);
        plain_chain[i-1].addChild(&plain_chain[i]);
    }
    // Cross the former 32-bit boundary as part of the production-derived test.
    LLView::sTreeGeneration=(U64{1}<<32)-1;
    probes=0;
    for(int frame=0;frame<frames;++frame) {
        for(int n=0;n<mutations;++n) {
            churn_root.addChild(&churn_child); churn_root.removeChild(&churn_child);
        }
        d.getBackgroundOpacity(); other.getBackgroundOpacity();
        for(int i=0;i<depth;++i) {
            expect(owned_chain[i].getBackgroundOpacity()==d.mBackgroundOpacity);
            expect(plain_chain[i].getBackgroundOpacity()==1.f);
        }
    }
    constexpr int bound=2*frames*(2*depth+1);
    std::printf("churn ancestor probes: %d; bound %d; %d views, %d paints, %d mutations/paint\n",
        probes,bound,2*depth,frames,mutations);
    expect(probes<=bound);
    expect(LLView::sTreeGeneration>(U64{1}<<32));
    // Branching backgrounds beneath never-drawn/background-invisible containers:
    // only sibling leaves call the accessor, even after unrelated invalidations.
    constexpr int siblings=384;
    std::array<LLView,siblings> owned_leaves, plain_leaves;
    for(int i=0;i<siblings;++i) {
        owned_chain.back().addChild(&owned_leaves[i]);
        plain_chain.back().addChild(&plain_leaves[i]);
    }
    probes=0;
    for(int frame=0;frame<frames;++frame) {
        for(int n=0;n<mutations;++n) {
            churn_root.addChild(&churn_child); churn_root.removeChild(&churn_child);
        }
        for(int i=0;i<siblings;++i) {
            expect(owned_leaves[i].getBackgroundOpacity()==d.mBackgroundOpacity);
            expect(plain_leaves[i].getBackgroundOpacity()==1.f);
        }
        // Inspect stamps/weak handles directly; do not warm intermediate caches.
        for(int i=0;i<depth;++i) {
            expect(owned_chain[i].mBackgroundOpacityResolved &&
                owned_chain[i].mBackgroundOpacityGeneration==LLView::sTreeGeneration &&
                owned_chain[i].mBackgroundOpacityOwner.get()==&d);
            expect(plain_chain[i].mBackgroundOpacityResolved &&
                plain_chain[i].mBackgroundOpacityGeneration==LLView::sTreeGeneration &&
                plain_chain[i].mBackgroundOpacityOwner.get()==nullptr);
        }
    }
    // Count both discovery and compression parent steps, including null-root steps.
    constexpr int branch_bound=2*frames*(2*(depth+siblings)+1);
    std::printf("branching ancestor probes: %d; bound %d; %d invisible ancestors, %d siblings/tree, %d paints\n",
        probes,branch_bound,depth,siblings,frames);
    expect(probes<=branch_bound);
    const std::array<std::array<F32,5>,6> samples{{
        {1,.2f,1,1,0}, {1,.3f,.4f,0,0}, {1,0,0,0,0},
        {1,.1f,.8f,0,.9f}, {1,0,1,0,1}, {0,.3f,0,0,0}}};
    for (F32 native : {1.f,.95f}) {
        d.current=native;
        for (auto sample : samples) {
            std::array<F32,5> base{{sample[0],sample[1]*native,sample[2]*native,
                sample[3]*(native==1.f?native:0.f),sample[4]*native}};
            auto reference=composite(base);
            F32 previous=-1.f;
            for(int step=0;step<=100;++step) {
                setting=step/100.f; d.drawRefresh(); painted_backing=0.f;
                d.drawBackgroundUnderlay();
                F32 bg=d.getBackgroundTransparency();
                std::array<F32,5> faded{{sample[0]*painted_backing,sample[1]*bg,
                    sample[2]*bg,sample[3]*(native==1.f?bg:0.f),sample[4]*bg}};
                auto pixel=composite(faded);
                if(step==100) expect(pixel.rgb==reference.rgb && pixel.coverage==reference.coverage);
                if(step==0) {
                    expect(pixel.coverage==0.f);
                    for(F32 alpha:faded) expect(alpha==0.f);
                }
                expect(pixel.coverage+1.e-6f>=previous);
                previous=pixel.coverage;
            }
        }
    }
    std::printf("native Director fade: %s (live/clamp/scope/cache/reparent/dead-owner; active/inactive composite identity/zero/monotonic)\n",failures?"FAIL":"PASS");
    return failures?1:0;
}
'''
    result = compile_run(check_directory('director_fade'), program)
    if negative:
        print(result.stdout, end='')
        return result.returncode != 0
    print(result.stdout, end='')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return True


def run_check():
    src = sources()
    errors = check(src)
    for error in errors:
        print('FAIL:', error)
    if errors:
        return 1
    native_check(src)
    print('director_fade_check: PASS')
    return 0


def selftest():
    src = sources()
    fixtures = (
        ('path compression removed', UI + 'llview.cpp',
         'ancestor = ancestor->mParentView;', 'break;'),
        ('floater extra alpha factor', UI + 'llfloater.cpp',
         'overlay_color % background_alpha)', 'overlay_color % background_alpha % 0.5f)'),
        ('ancestor cache reuse removed', UI + 'llview.cpp',
         'owner != this && owner->mBackgroundOpacityResolved &&', 'false &&'),
        ('drawItems alpha comment removed', UI + 'llscrolllistctrl.cpp',
         '// Local alpha excludes background opacity: stripes/disabled rows are pre-multiplied once.', ''),
        ('kit fill bypass', NV + 'alkitcard.cpp', ACCESSOR, ''),
        ('new unclassified kit background', NV + 'alkitpage.cpp', 'LLPanel::draw();',
         'gl_rect_2d(getLocalRect(), LLColor4::black, true); LLPanel::draw();'),
        ('leak to ordinary floater', UI + 'llview.cpp', ': 1.f;', ': .5f;'),
        ('control alpha multiplied', NV + 'alkitrow.cpp', 'ALKitStyle::INPUT_BG).get(), true)',
         'ALKitStyle::INPUT_BG).get()' + ACCESSOR + ', true)'),
        ('text alpha multiplied', NV + 'alkitcard.cpp', 'ALKitStyle::TEXT_STRONG).get(), LLFontGL::LEFT',
         'ALKitStyle::TEXT_STRONG).get()' + ACCESSOR + ', LLFontGL::LEFT'),
        ('old backing quad', NV + 'llfloaterdirector.cpp', '    LLFloater::draw();',
         '    gl_rect_2d(getLocalRect(), ALKitStyle::color(ALKitStyle::WINDOW_BG).get(), true);\n    LLFloater::draw();'),
        ('backing removed', NV + 'llfloaterdirector.cpp',
         'gl_rect_2d(getLocalRect(), backing, true);', ''),
        ('backing left unfaded', NV + 'llfloaterdirector.cpp',
         'backing.mV[VALPHA] = getBackgroundOpacity();', 'backing.mV[VALPHA] = 1.f;'),
        ('shared text alpha multiplied', UI + 'lltextbase.cpp',
         '% (alpha * mStyle->getAlpha())', '% (alpha * mStyle->getAlpha() * mEditor.getBackgroundOpacity())'),
        ('shared selection alpha multiplied', UI + 'lltextbase.cpp',
         'alpha *= getDrawContext().mAlpha;', 'alpha *= getDrawContext().mAlpha * getBackgroundOpacity();'),
        ('shared control face alpha multiplied', UI + 'llbutton.cpp',
         'F32 alpha = mUseDrawContextAlpha ? getDrawContext().mAlpha : getCurrentTransparency();',
         'F32 alpha = (mUseDrawContextAlpha ? getDrawContext().mAlpha : getCurrentTransparency()) * getBackgroundOpacity();'),
        ('cache not invalidated on add/reparent', UI + 'llview.cpp',
         'mChildList.push_front(child);\n    ++sTreeGeneration;', 'mChildList.push_front(child);'),
        ('reparented-out view keeps Director alpha', UI + 'llview.cpp',
         'child->mParentView = NULL;\n        ++sTreeGeneration;', 'child->mParentView = NULL;'),
        ('global popup takes Director alpha', UI + 'llview.cpp',
         'return owner ? owner->getOwnBackgroundOpacity() : 1.f;',
         'return owner ? owner->getOwnBackgroundOpacity() : .25f;'),
        ('default not one', NV + 'app_settings/settings.xml', '<real>1.0</real>', '<real>0.5</real>'),
        ('list stripe bypass', UI + 'llscrolllistctrl.cpp', 'mBgStripeColor.get() % getBackgroundOpacity()', 'mBgStripeColor.get()'),
        ('read-only document bypass', UI + 'lltextbase.cpp', 'mReadOnly ? getBackgroundTransparency() : getCurrentTransparency()', 'getCurrentTransparency()'),
        ('cached default not one', NV + 'llfloaterdirector.cpp', '"DirectorBackgroundOpacity", 1.f', '"DirectorBackgroundOpacity", .5f'),
    )
    failures = check(src)
    native_check(src)
    for label, path, old, new in fixtures:
        changed = copy.copy(src)
        if label == 'default not one':
            # Mutate this setting only, not the first unrelated default in settings.xml.
            before, after = changed[path].split('<key>DirectorBackgroundOpacity</key>', 1)
            assert old in after
            changed[path] = before + '<key>DirectorBackgroundOpacity</key>' + after.replace(old, new, 1)
        else:
            assert old in changed[path], label
            changed[path] = changed[path].replace(old, new, 1)
        if not check(changed):
            failures.append('planted negative escaped: ' + label)
        else:
            print('ok    rejected ' + label)
        if label in ('path compression removed', 'ancestor cache reuse removed', 'backing removed', 'backing left unfaded',
                     'reparented-out view keeps Director alpha', 'global popup takes Director alpha'):
            if not native_check(changed, negative=True):
                failures.append('native semantic negative escaped: ' + label)
            else:
                print('ok    native rejected ' + label)
    for failure in failures:
        print('FAIL:', failure)
    print(f'director_fade_check selftest: {"FAIL" if failures else "PASS"}')
    return bool(failures)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--selftest', action='store_true')
    args = parser.parse_args()
    raise SystemExit(selftest() if args.selftest else run_check())
