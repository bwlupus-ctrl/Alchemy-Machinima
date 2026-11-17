"""WP3 R1-R17 source contracts, table-A census, and named planted source negatives."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[2]; NV=ROOT/'indra/newview'

def code(s):
    # Keep string contents (action/settings contracts) but remove comments, preserving line positions.
    return re.sub(r'("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')|//[^\n]*|/\*[\s\S]*?\*/',lambda m:m[1] or ' '*len(m[0]),s)
def body(s,signature):
    start=s.index('{',s.index(signature)); mask=re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),s)
    depth=0
    for i in range(start,len(s)):
        depth+=(mask[i]=='{')-(mask[i]=='}')
        if depth==0:return s[start:i+1]
    raise AssertionError('unclosed '+signature)
def outside_clean(s,token):
    for match in re.finditer(r'if\s*\(\s*!ALCleanFrame::active\(\)[^;{]*\{',s):
        start=match.start(); block=body(s,s[start:s.index('{',start)])
        if token in block:return False
    return all(not re.search(r'if\s*\(\s*!ALCleanFrame::active',line[:line.index(token)]) for line in s.splitlines() if token in line)
def census(files, token=None):
    result={}
    if token is None:token=re.compile(r'\b(?:hasRenderDebugFeatureMask|hasRawRenderDebugFeatureMask|getIndicatorsVisible)\s*\(|\bRENDER_DEBUG_FEATURE_UI\b')
    for name,text in files.items():
        if not token.search(text):continue
        # Build function ranges from lexical braces, retaining statements and owner
        # identities: moving an identical call into another function is a new caller.
        mask=re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),text)
        stack=[];ends={}
        for i,c in enumerate(mask):
            if c=='{':stack.append(i)
            elif c=='}' and stack:ends[stack.pop()]=i
        functions=[]
        pattern=r'(?m)^[ \t]*(?:[\w:<>,*&~]+[ \t]+)*([\w:~]+)[ \t]*\([^;{}]*?\)[ \t\r\n]*(?:const[ \t\r\n]*)?(?:override[ \t\r\n]*)?\{'
        for m in re.finditer(pattern,mask):
            owner=m[1]
            if owner in ('if','for','while','switch','catch'):continue
            opening=mask.index('{',m.start())
            if opening in ends:functions.append((opening,ends[opening],owner))
        offset=0
        for line in text.splitlines(keepends=True):
            if token.search(line):
                candidates=[f for f in functions if f[0]<=offset+len(line) and f[1]>=offset]
                owner=min(candidates,key=lambda f:f[1]-f[0])[2] if candidates else '<declaration>'
                statement=re.sub(r'\s+',' ',line.strip())
                key=name+' :: '+owner+' :: '+statement
                result[key]=result.get(key,0)+1
            offset+=len(line)
    return dict(sorted(result.items()))
# Binding table-A census: effective/raw/indicator/UI-token counts. Any new caller or
# changed classification fails, even inside an already-listed file. Other-bit consumers
# are pinned too (row 34); comments do not create fictitious consumers.
CENSUS={'alghoststudio.cpp :: ALGhostStudio::updateNameplates :: gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'alpanelactorkey.cpp :: ALActorKeyUI::drawBadge :: !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;': 1, 'aluiperfselftest.cpp :: functionalInputSafe :: if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'aluiperfselftest.cpp :: runFunctionalSentinel :: bool ok = gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI) &&': 1, 'llactormover.cpp :: LLActorMover::renderStudioGhosts :: (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI) && gViewerWindow->getUIVisibility())) &&': 1, 'llflexibleobject.cpp :: LLVolumeImplFlexible::doFlexibleUpdate :: if (!force_update || !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FLEXIBLE))': 1, 'llflexibleobject.cpp :: LLVolumeImplFlexible::doIdleUpdate :: if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FLEXIBLE))': 1, 'llhudeffecttrail.cpp :: LLHUDEffectSpiral::render :: (gViewerWindow && !gViewerWindow->getIndicatorsVisible()))) )': 1, 'lllegacyatmospherics.cpp :: LLAtmospherics::updateFog :: if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FOG))': 1, 'llpipelinelistener.cpp :: has_render_feature_wrapper :: response["value"] = gPipeline.hasRawRenderDebugFeatureMask(render_feature);': 1, 'llviewercontrol.cpp :: handleRenderPerfTestChanged :: gPipeline.setRenderDebugFeatureControl(LLPipeline::RENDER_DEBUG_FEATURE_UI, false);': 1, 'llviewercontrol.cpp :: handleRenderPerfTestChanged :: gPipeline.setRenderDebugFeatureControl(LLPipeline::RENDER_DEBUG_FEATURE_UI, true);': 1, 'llviewerdisplay.cpp :: display :: if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))': 1, 'llviewerdisplay.cpp :: render_hud_attachments :: bool has_ui = gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerdisplay.cpp :: render_hud_attachments :: gPipeline.toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 2, 'llviewerdisplay.cpp :: render_ui :: bool render_ui = gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerdisplay.cpp :: render_ui_3d :: if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI) &&': 1, 'llviewermenu.cpp :: feature_from_string :: return LLPipeline::RENDER_DEBUG_FEATURE_UI;': 1, 'llviewerpartsim.cpp :: LLViewerPartSim::updateSimulation :: !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)))': 1, 'llviewertexture.cpp :: update_capture_mode :: if (on_hide_ui && !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.cpp :: LLViewerWindow::cubeSnapshot :: LLPipeline::toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 2, 'llviewerwindow.cpp :: LLViewerWindow::cubeSnapshot :: bool prev_draw_ui = gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerwindow.cpp :: LLViewerWindow::cubeSnapshot :: if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.cpp :: LLViewerWindow::draw :: if (!gSnapshot && getIndicatorsVisible())': 1, 'llviewerwindow.cpp :: LLViewerWindow::getIndicatorsVisible :: bool LLViewerWindow::getIndicatorsVisible()': 1, 'llviewerwindow.cpp :: LLViewerWindow::getIndicatorsVisible :: return mUIVisible && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerwindow.cpp :: LLViewerWindow::handleAnyMouseClick :: if(gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.cpp :: LLViewerWindow::rawSnapshot :: LLPipeline::toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 2, 'llviewerwindow.cpp :: LLViewerWindow::rawSnapshot :: bool prev_draw_ui = gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerwindow.cpp :: LLViewerWindow::renderSelections :: if (!for_gl_pick && !getIndicatorsVisible())': 1, 'llviewerwindow.cpp :: LLViewerWindow::simpleSnapshot :: LLPipeline::toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 2, 'llviewerwindow.cpp :: LLViewerWindow::simpleSnapshot :: bool prev_draw_ui = gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerwindow.cpp :: LLViewerWindow::simpleSnapshot :: if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.cpp :: LLViewerWindow::updateKeyboardFocus :: if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.cpp :: LLViewerWindow::updateLayout :: const bool hide_cursor = ALCleanFrame::active() || ALHideCursor::wantHidden(hide_cursor_with_ui, !getIndicatorsVisible(), true, true);': 1, 'llviewerwindow.cpp :: LLViewerWindow::updateUI :: bool world_view_uses_full_window = gAgentCamera.cameraMouselook() || !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);': 1, 'llviewerwindow.cpp :: LLViewerWindow::updateUI :: if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 2, 'llviewerwindow.cpp :: update :: if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'llviewerwindow.h :: <declaration> :: bool getIndicatorsVisible();': 1, 'pipeline.cpp :: LLPipeline::hasRawRenderDebugFeatureMask :: bool LLPipeline::hasRawRenderDebugFeatureMask(const U32 mask) const': 1, 'pipeline.cpp :: LLPipeline::renderActorPathGuides :: !hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI) ||': 1, 'pipeline.cpp :: LLPipeline::renderCompositionGuideOverlay :: if (!gViewerWindow->getIndicatorsVisible() && !with_ui_hidden)': 1, 'pipeline.cpp :: LLPipeline::renderFinalize :: if (guide_work && hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI) && gViewerWindow->getUIVisibility())': 1, 'pipeline.cpp :: LLPipeline::renderFocusPoint :: if (sDoFEnabled && render_focus_point_crosshair && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'pipeline.cpp :: LLPipeline::renderHighlights :: gViewerWindow && gViewerWindow->getIndicatorsVisible())': 2, 'pipeline.cpp :: LLPipeline::renderHighlights :: if (!ALCleanFrame::active() && hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED) &&': 2, 'pipeline.cpp :: LLPipeline::toggleRenderDebugFeatureControl :: return gPipeline.hasRawRenderDebugFeatureMask(bit);': 1, 'pipeline.cpp :: render_hud_elements :: if (!LLPipeline::sReflectionRender && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))': 1, 'pipeline.h :: <declaration> :: RENDER_DEBUG_FEATURE_UI = 0x0001,': 1, 'pipeline.h :: <declaration> :: bool hasRawRenderDebugFeatureMask(const U32 mask) const;': 1, 'pipeline.h :: hasRenderDebugFeatureMask :: bool hasRenderDebugFeatureMask(const U32 mask) const { return bool(mRenderDebugFeatureMask & mask & ~(ALCleanFrame::active() ? RENDER_DEBUG_FEATURE_UI : 0u)); }': 1}

# 2026-11-17: WP6 overlay is table-A row 7, with its own snapshot/Prism/reflection/clean guards.
CENSUS['alkeyframeoverlay.cpp :: ALKeyframeOverlay::draw :: !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;'] = 1
# C8: stock rawSnapshot forces UI on after capture, including when it began off.
CENSUS['llviewerwindow.cpp :: LLViewerWindow::rawSnapshot :: if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))'] = 1

CLEAN_TOKEN=re.compile(r'\bALCleanFrame\s*::\s*active\s*\(')
GUIDE_TOKEN=re.compile(r'\b(?:drawGuides|renderActorPathGuides)\s*\(')
# R19/R20 exact caller/statement pins are below; new files/functions/sites fail.
CLEAN_CENSUS={'alcleantelemetry.h :: guard :: static void guard(Site s, bool work) { if(ALCleanFrame::active()) { ++sites[s].evaluated; sites[s].blocked += work; } }': 1,
 'alcleantelemetry.h :: submit :: static void submit(Site s) { if(ALCleanFrame::active()) ++sites[s].forbidden; }': 1,
 'alkeyframeoverlay.cpp :: ALKeyframeOverlay::draw :: if(!sBuffer || ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive() ||': 1,
 'alkeyframeplayer.cpp :: ALKeyframePlayer::drawGuides :: if(!ALCleanFrame::active() && hasGuideDraw()) mImpl->guideDraw();': 1,
 'alkeyframeplayer.cpp :: ALKeyframePlayer::hasGuideDraw :: bool ALKeyframePlayer::hasGuideDraw() const { return bool(mImpl->guideDraw) && !ALCleanFrame::active() && !rlvHandbackActive(); }': 1,
 'alkeyframeplayer.cpp :: ALKeyframePlayer::presented :: if(ALCleanFrame::active() && mImpl->lastPresented!=mImpl->generation)': 1,
 'alkeyframetimeline.cpp :: ALKeyframeTimeline::handleMouseDown :: if(ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive()) return true;': 1,
 'alpanelkeyframes.cpp :: ALPanelKeyframes::idle :: if(ALCleanFrame::active()) return;': 1,
 'alpanelkeyframes.cpp :: ALPanelKeyframes::refresh :: else if(name=="kf_visit"||name=="kf_playhead")enabled=!restricted&&!p.rlvHandbackActive()&&!ALCleanFrame::active()&&(name!="kf_visit"||key);': 1,
 'alpanelkeyframes.cpp :: ALPanelKeyframes::refresh :: if(!mTimeline||ALCleanFrame::active()) return;': 1,
 'alpanelkeyframes.cpp :: ALPanelKeyframes::refresh :: if(name=="kf_playing")enabled=!restricted||ALCleanFrame::active();': 1,
 'alpanelkeyframes.cpp :: handleMouseDown :: if(ALCleanFrame::active()) return true;': 1,
 'altoolkeyframeedit.cpp :: allowed :: return !ALCleanFrame::active() && !p.ownsCamera() && !p.rlvHandbackActive() && !p.restricted(why);': 1,
 'llactormover.cpp :: LLActorMover::renderStudioGhosts :: if (!ALCleanFrame::active() && (!ALActorChromaKey::sActive || !ALActorChromaKey::keyedThisFrame() ||': 1,
 'lldrawpoolalpha.cpp :: LLDrawPoolAlpha::renderAlpha :: if(ALCleanFrame::active() && is_particle_or_hud_particle && ALKeyframeViewer::particleGroupForbidden(group))': 1,
 'llviewerdisplay.cpp :: render_hud_attachments :: if (!ALCleanFrame::active() && LLPipeline::sShowHUDAttachments && !gDisconnected && setup_hud_matrices())': 1,
 'llviewerdisplay.cpp :: render_ui :: if (!ALCleanFrame::active()) gViewerWindow->drawDebugText();': 1,
 'llviewerdisplay.cpp :: render_ui_3d :: if (!ALCleanFrame::active() && show_axes())': 1,
 'llviewerinput.cpp :: LLViewerInput::handleGlobalBindsMouse :: if(down ? sCleanGlobalMouse.mouseDown(button,ALCleanFrame::active()) : sCleanGlobalMouse.mouseUp(button)) return true;': 1,
 'llviewerinput.cpp :: LLViewerInput::handleMouse :: if(down ? sCleanMouse.mouseDown(button,ALCleanFrame::active()) : sCleanMouse.mouseUp(button)) return true;': 1,
 'llviewerinput.cpp :: LLViewerInput::scanKey :: const bool clean=ALCleanFrame::active();': 1,
 'llviewerpartsim.cpp :: LLViewerPartSim::updateSimulation :: if (ALCleanFrame::active() || (hide_effect_particles &&': 1,
 'llviewerwindow.cpp :: LLViewerWindow::drawDebugText :: if (!gSnapshot && !ALCleanFrame::active())': 1,
 'llviewerwindow.cpp :: LLViewerWindow::drawDebugText :: if(ALCleanFrame::active()) return;': 1,
 'llviewerwindow.cpp :: LLViewerWindow::handleDragNDrop :: if (gViewerInput.worldInputSuspended() || ALCleanFrame::active()) return result;': 1,
 'llviewerwindow.cpp :: LLViewerWindow::handleKey :: if (ALCleanFrame::active())': 1,
 'llviewerwindow.cpp :: LLViewerWindow::handleScrollHWheel :: if(ALCleanFrame::active()) return;': 1,
 'llviewerwindow.cpp :: LLViewerWindow::handleScrollWheel :: if(ALCleanFrame::active()) return;': 1,
 'llviewerwindow.cpp :: LLViewerWindow::handleUnicodeChar :: if(ALCleanFrame::active()) return true;': 1,
 'llviewerwindow.cpp :: LLViewerWindow::rawSnapshot :: if(ALCleanFrame::active()) { LL_INFOS("Keyframes") << "rawSnapshot deferred during Play" << LL_ENDL; return false; }': 1,
 'llviewerwindow.cpp :: LLViewerWindow::simpleSnapshot :: if(ALCleanFrame::active()) { LL_INFOS("Keyframes") << "simpleSnapshot deferred during Play" << LL_ENDL; return false; }': 1,
 'llviewerwindow.cpp :: LLViewerWindow::updateLayout :: const bool hide_cursor = ALCleanFrame::active() || ALHideCursor::wantHidden(hide_cursor_with_ui, !getIndicatorsVisible(), true, true);': 1,
 'llviewerwindow.cpp :: LLViewerWindow::updateUI :: const bool world_input_suspended = gViewerInput.worldInputSuspended() || ALCleanFrame::active();': 1,
 'llvopartgroup.cpp :: ALKeyframeViewer::cleanParticleTransition :: ALCleanParticleBatches::transition(sKeyframeParticleObjects,ALCleanFrame::active(),': 1,
 'llvopartgroup.cpp :: ALKeyframeViewer::particleGroupBuilt :: if(ALCleanFrame::active()) ALCleanParticleBatches::built(reinterpret_cast<std::uintptr_t>(group),forbidden);': 1,
 'llvopartgroup.cpp :: LLParticlePartition::addGeometryCount :: if(ALCleanFrame::active())': 1,
 'llvopartgroup.cpp :: LLParticlePartition::addGeometryCount :: if(count || !ALCleanFrame::active()) obj->mDepth /= count;': 1,
 'llvopartgroup.cpp :: LLVOPartGroup::updateGeometry :: const bool effect=ALCleanFrame::active() && keyframeEffect(*part);': 1,
 'llvopartgroup.cpp :: LLVOPartGroup::updateGeometry :: if (!ALCleanFrame::active() && !(gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES)))': 1,
 'llvopartgroup.cpp :: LLVOPartGroup::updateGeometry :: if (!ALCleanParticleBatches::visible(effect,ALCleanFrame::active()) ||': 1,
 'llvopartgroup.cpp :: LLVOPartGroup::updateGeometry :: if(ALCleanFrame::active()) sKeyframeParticleObjects.find(this)->second=effect_geometry;': 1,
 'pipeline.cpp :: LLPipeline::postSort :: if (!ALCleanFrame::active() && LLFloaterReg::instanceVisible("beacons") && !sShadowRender && !gCubeSnapshot)': 1,
 'pipeline.cpp :: LLPipeline::renderCompositionGuideOverlay :: if(ALCleanFrame::active()) return;': 1,
 'pipeline.cpp :: LLPipeline::renderDebug :: if (!ALCleanFrame::active())': 2,
 'pipeline.cpp :: LLPipeline::renderDebug :: if (ALCleanFrame::active() && !hasRenderType(RENDER_TYPE_HUD))': 1,
 'pipeline.cpp :: LLPipeline::renderHighlights :: if (!ALCleanFrame::active() && hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED) &&': 2,
 'pipeline.cpp :: renderMOAPBeacons :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: renderParticleBeacons :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: renderPhysicalBeacons :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: renderScriptedBeacons :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: renderScriptedTouchBeacons :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: renderSoundHighlights :: if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)': 1,
 'pipeline.cpp :: render_hud_elements :: else if (!ALCleanFrame::active() && gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))': 1,
 'pipeline.cpp :: render_hud_elements :: else if (gForceRenderLandFence && !ALCleanFrame::active())': 1,
 'pipeline.h :: hasRenderDebugFeatureMask :: bool hasRenderDebugFeatureMask(const U32 mask) const { return bool(mRenderDebugFeatureMask & mask & ~(ALCleanFrame::active() ? RENDER_DEBUG_FEATURE_UI : 0u)); }': 1}
GUIDE_CENSUS={'alkeyframeplayer.cpp :: ALKeyframePlayer::drawGuides :: void ALKeyframePlayer::drawGuides() const': 1,
 'alkeyframeplayer.h :: <declaration> :: void drawGuides() const;': 1,
 'pipeline.cpp :: <declaration> :: void LLPipeline::renderActorPathGuides(LLRenderTarget* target, const glm::mat4& actor_path_projection,': 1,
 'pipeline.cpp :: LLPipeline::FinalizeGuides::operator :: pipeline.renderActorPathGuides(target, projection, modelview, attachment);': 1,
 'pipeline.cpp :: LLPipeline::renderActorPathGuides :: ALKeyframePlayer::instance().drawGuides();': 1,
 'pipeline.h :: <declaration> :: void renderActorPathGuides(LLRenderTarget* target, const glm::mat4& projection,': 1}

def assertions(f):
    errors=[]
    def need(n,condition):
        if not condition:errors.append('R'+str(n))
    app=f['llappviewer.cpp']; player=f['alkeyframeplayer.cpp']; window=f['llviewerwindow.cpp']; inp=f['llviewerinput.cpp']; pipe=f['pipeline.cpp']; trans=f['alkeyframetransport.cpp']; display=f['llviewerdisplay.cpp']
    idle=app[app.index('const ECameraOwner previous_camera_owner'):]
    need(1,app.count('ALKeyframePlayer::instance().lifecycleCheck();')==2 and app.index('ALKeyframePlayer::instance().lifecycleCheck();')<app.index('ALProjectorTracker::instance().clear();',app.index('void LLAppViewer::idle')) and idle.index('ECameraOwner::PILOT')<idle.index('ECameraOwner::KEYFRAME')<idle.index('ECameraOwner::RECORDER') and 'ALKeyframePlayer::instance().onDisconnect();' in app and 'ALKeyframePlayer::instance().shutdown();' in app)
    need(2,app.count('ALKeyframePlayer::instance().advance(LLPresentationTime::currentFrame());')==1 and 'mAdvanced && mGeneration == frame.generation' in trans and 'p.advanced && p.generation==frame.generation' in player)
    need(3,f['llcinematiccamera.cpp'].count('ALKeyframePlayer::instance().ownsCamera()')==2 and f['llcinematiccamera.cpp'].count('ALKeyframePlayer::instance().rlvHandbackActive()')==2)
    need(4,'ALKeyframePlayer::instance().drawGuides();' in body(pipe,'void LLPipeline::renderActorPathGuides(') and 'const bool guide_work = ALKeyframePlayer::instance().hasGuideDraw() ||' in pipe and census(f)==CENSUS and 'return mUIVisible && gPipeline.hasRenderDebugFeatureMask' in window and 'ALCleanFrame::active() ? RENDER_DEBUG_FEATURE_UI : 0u' in f['pipeline.h'] and 'hasRawRenderDebugFeatureMask(render_feature)' in f['llpipelinelistener.cpp'] and 'return gPipeline.hasRawRenderDebugFeatureMask(bit);' in pipe and '!ALCleanFrame::active() && LLPipeline::sShowHUDAttachments' in display and '!ALCleanFrame::active() && show_axes()' in display and '!ALCleanFrame::active()) gViewerWindow->drawDebugText()' in display and '!ALCleanFrame::active() && (!ALActorChromaKey::sActive' in f['llactormover.cpp'] and 'ALCleanFrame::active() || ALHideCursor::wantHidden' in window and 'if(ALCleanFrame::active()) return;' in body(pipe,'void LLPipeline::renderCompositionGuideOverlay()') and 'if(ALCleanFrame::active()) return;' in body(window,'void LLViewerWindow::drawDebugText()') and 'else if (!ALCleanFrame::active() && gPipeline.hasRenderType' in pipe)
    need(5,'if(fx.resetFlycam) LLViewerJoystick::getInstance()->setNeedsReset(true);' in player and 'State::HOLD_END' in trans and 'State::PREVIEW' in player and 'Exit::USER_INPUT' in player)
    need(6,'sendViewToSimulator(vertical_fov_rads);' in body(f['llviewercamera.cpp'],'void LLViewerCamera::setView(') and 'setViewNoBroadcast(fov);' in f['alkeyframecamera.cpp'] and 'gDisconnected || !gAgent.getRegion() || !gMessageSystem' in player and 'wall-lastSent>=1' in player and '2*ALKf::degree' in player)
    need(7,f['llflycamrecorder.cpp'].count('stop(ALKfTransport::Exit::RECORDER)')==2 and 'LLFlycamRecorder::instance().stopPlayback();' in player and 'arm_play && !arm_keyframes' in f['lldirectorcast.cpp'] and 'if (arm_keyframes) ALKeyframePlayer::instance().play(true);' in f['lldirectorcast.cpp'] and 'stop(ALKfTransport::Exit::CUT)' in body(f['lldirectorcast.cpp'],'void LLDirectorCast::cut()'))
    acquire=player[player.index('if(fx.acquire)'):player.index('if(fx.release)')]
    need(8,all(x in acquire for x in ('ALCleanFrame::setActive(true)','p.cancelEdit()','setMouseCapture(NULL)','hideMenus()','blockToolTips()','hideInstance("keybind_dialog")','setKeyboardFocus(NULL)','cleanParticleTransition()')) and 'ALCleanFrame::setActive(false); ALKeyframeViewer::cleanParticleTransition();' in player)
    need(9,all(x in f['alkeyframecamera.cpp'] for x in ('isCameraPresetLocked()','RLV_BHVR_SETCAM_UNLOCK','isCameraDistanceClamped()','isCameraFOVClamped()','sRestrictionDirty')) and 'setBehaviourToggleCallback' in player and 'if(fx.resetAgent) gAgentCamera.resetView(true,false);' in player and 'mHandback && !l.restricted' in trans)
    need(10,'runtime.wallDelta=p.wallDelta;' in player and 'e.cache.revision!=track.revision' in player and 'e.uid!=track.uid' in player and 'frame.wall_delta' in player and 'std::array<double, 4096>' in f['alcleantelemetry.h'] and 'forbidden_geometry' not in body(f['llvopartgroup.cpp'],'void LLParticlePartition::getGeometry('))
    key=body(window,'bool LLViewerWindow::handleKey(KEY key, MASK mask)')
    need(11,'if (ALCleanFrame::active())' in key and key.index('if (ALCleanFrame::active())')<key.index('LLSetKeyBindDialog::recordKey') and all(x in key for x in ('KEY_F3','KEY_F4','triggerGesture(key,mask)','gGestureList.trigger(key,mask)','return false;')) and 'if(ALCleanFrame::active()) return true;' in body(window,'bool LLViewerWindow::handleUnicodeChar('))
    raw=body(window,'bool LLViewerWindow::rawSnapshot('); simple=body(window,'bool LLViewerWindow::simpleSnapshot('); cube=body(window,'bool LLViewerWindow::cubeSnapshot(')
    need(12,'if (prev_draw_ui != show_ui)' in raw and 'toggled_ui' not in raw and raw.count('LLPipeline::toggleRenderDebugFeature(LLPipeline::RENDER_DEBUG_FEATURE_UI);')==2 and 'if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))' in raw[raw.rindex('gDepthDirty = true;'):] and 'if(ALCleanFrame::active())' in raw and 'if(ALCleanFrame::active())' in simple and 'if(ALCleanFrame::active())' not in cube and all('hasRenderDebugFeatureMask(' not in b for b in (raw,simple,cube)) and 'if (prev_draw_ui)' in simple and 'if (prev_draw_ui)' in cube)
    need(13,'const bool world_input_suspended = gViewerInput.worldInputSuspended() || ALCleanFrame::active();' in window and 'worldInputSuspended() || ALCleanFrame::active()) return result;' in window and 'if(ALCleanFrame::active()) return;' in body(window,'void LLViewerWindow::handleScrollWheel(LLScrollDelta') and 'if(ALCleanFrame::active()) return;' in body(window,'void LLViewerWindow::handleScrollHWheel(LLScrollDelta'))
    dispatch=idle[:idle.index('else if (LLFlycamRecorder::instance().isPlaybackActive())')]
    need(14,'ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()' in dispatch and 'gCameraDispatchOwner = ECameraOwner::AGENT;' in dispatch and 'gAgentCamera.updateCamera();' in dispatch)
    highlights=body(pipe,'void LLPipeline::renderHighlights()'); debug=body(pipe,'void LLPipeline::renderDebug()')
    need(15,highlights.count('mHighlightFaces.clear();')==1 and '!ALCleanFrame::active() && hasRenderDebugFeatureMask' in highlights and outside_clean(highlights,'mHighlightFaces.clear();') and outside_clean(debug,'visible_selected_groups.clear();') and outside_clean(debug,'mDebugBlips.erase(it)') and 'if (ALCleanFrame::active() && !hasRenderType(RENDER_TYPE_HUD))' in debug and outside_clean(body(pipe,'void LLPipeline::renderGeomPostDeferred('),'mHighlightFaces.clear();') and 'if (!ALCleanFrame::active() && LLFloaterReg::instanceVisible("beacons")' in pipe and pipe.count('if (!ALCleanFrame::active() && gPipeline.sRenderHighlight)')>=6)
    particles=f['llvopartgroup.cpp']; alpha=f['lldrawpoolalpha.cpp']
    need(16,'if (ALCleanFrame::active() || (hide_effect_particles &&' in f['llviewerpartsim.cpp'] and 'const bool effect=ALCleanFrame::active() && keyframeEffect(*part);' in particles and 'if (!ALCleanParticleBatches::visible(effect,ALCleanFrame::active()) ||' in particles and 'facep->setSize(0, 0);' in particles and all(x in body(particles,'void ALKeyframeViewer::cleanParticleTransition()') for x in ('updateGeometry(drawable)','rebuildGeom(group)','rebuildMesh()')) and 'particleGroupForbidden(group)' in alpha and 'submit(ALCleanTelemetry::EffectGeometry)' in alpha)
    need(17,'setCameraNeedsUpdate(true);' in dispatch and dispatch.index('setCameraNeedsUpdate(true);')<dispatch.index('gAgentCamera.updateCamera();'))
    killed=body(f['llviewermessage.cpp'],'void process_kill_object(')
    need(18, 'ALKeyframePlayer::instance().targetRemoved(ALKeyframeViewer::id(id));' in killed and
         killed.index('getUUIDFromLocal') < killed.index('targetRemoved') < killed.index('gObjectList.killObject(objectp)') and
         killed.index('BlockAttachmentKillsOnTeleport') < killed.index('targetRemoved') and
         'targetRemoved(' not in f['llviewerobjectlist.cpp'] and 'targetRemoved(' not in f['llviewerobject.cpp'])
    need(19,census(f,CLEAN_TOKEN)==CLEAN_CENSUS and
         sum(len(CLEAN_TOKEN.findall(text)) for text in f.values())==57)
    guides=body(pipe,'void LLPipeline::renderActorPathGuides(')
    overlay=body(f['alkeyframeoverlay.cpp'],'void ALKeyframeOverlay::draw()')
    main=body(pipe,'bool LLPipeline::isTagRecordView() const')
    need(20,census(f,GUIDE_TOKEN)==GUIDE_CENSUS and
         'if (isTagRecordView() && !sReflectionRender && !sShadowRender)\n            ALKeyframePlayer::instance().drawGuides();' in guides and
         # Feature-off identity: the existing Actor Mover/path guides keep baseline pass
         # eligibility (snapshots included); only the keyframe draw is main-view restricted.
         guides.count('isTagRecordView')==1 and guides.index('renderActorPathOverlay')<guides.index('isTagRecordView') and
         not any(x in guides[:guides.index('target->bindTarget();')] for x in ('isTagRecordView','sReflectionRender','sShadowRender')) and
         all(x in overlay[:overlay.index('gGL.flush();')] for x in ('!gPipeline.isTagRecordView()', 'LLPipeline::sShadowRender', 'LLPipeline::sReflectionRender')) and
         'return mRT == &mMainRT && !gSnapshot && !gCubeSnapshot && !gSnapshotNoPost &&' in main and
         '!sRenderingHUDs && !sImpostorRender && !sPrismLensRender && !sPrismPreviewRender;' in main)
    need(21,'if(key==KEY_F4 || ALKeyframePlayer::instance().ownsCamera() || cast.isRunning() || cast.isCountingDown()) cast.cut(); else cast.action();' in key)
    early='if (mask==MASK_NONE && keyframe_cut_hotkeys && (key==KEY_F3 || key==KEY_F4) &&'+chr(10)+'            (ALKeyframePlayer::instance().ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()))'+chr(10)+'        { LLDirectorCast::instance().cut(); return true; }'
    need(22,early in key and 0<=key.find('Exit::ESCAPE')<key.find(early) and key.find(early)<key.find('LLSetKeyBindDialog::recordKey') and key.find(early)<(key.find('if (ALCleanFrame::active())') if 'if (ALCleanFrame::active())' in key else 0))
    return errors

def check():
    files={}
    for p in (ROOT/'indra').rglob('*'):
        if p.suffix not in ('.cpp','.h','.hpp','.inl','.cc','.cxx','.hh','.hxx'):continue
        text=code(p.read_text(encoding='utf-8'))
        if NV in p.parents:files[p.relative_to(NV).as_posix()]=text
        elif CLEAN_TOKEN.search(text):files['../'+p.relative_to(ROOT/'indra').as_posix()]=text
    failures=assertions(files);assert not failures,failures
    print('PASS R1-R22 runtime; table-A census '+str(len(CENSUS))+' classified caller statements')
    print('R4 table A effective/raw/indicators/configuration/other-bit caller identities pinned')
    mutations=[
      (1,'llappviewer.cpp','    ALKeyframePlayer::instance().lifecycleCheck();',''),
      (2,'alkeyframetransport.cpp','if (mAdvanced && mGeneration == frame.generation) return;','if(false) return;'),
      (3,'llcinematiccamera.cpp','!ALKeyframePlayer::instance().ownsCamera()','true'),
      (4,'__unlisted.cpp','', 'bool leak(){return gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);}'),
      (5,'alkeyframeplayer.cpp','if(fx.resetFlycam) LLViewerJoystick::getInstance()->setNeedsReset(true);',''),
      (6,'alkeyframeplayer.cpp','wall-lastSent>=1','true'),
      (7,'lldirectorcast.cpp','arm_play && !arm_keyframes','arm_play'),
      (8,'alkeyframeplayer.cpp','gFocusMgr.setMouseCapture(NULL);',''),
      (9,'alkeyframecamera.cpp','RlvActions::isCameraFOVClamped()','false'),
      (10,'alkeyframeplayer.cpp','runtime.wallDelta=p.wallDelta;','runtime.wallDelta=.016;'),
      (11,'llviewerwindow.cpp','    if (ALCleanFrame::active())\n    {','    if (false)\n    {'),
      (12,'llviewerwindow.cpp','bool LLViewerWindow::cubeSnapshot(', 'bool LLViewerWindow::cubeSnapshot('),
      (13,'llviewerwindow.cpp','const bool world_input_suspended = gViewerInput.worldInputSuspended() || ALCleanFrame::active();','const bool world_input_suspended = gViewerInput.worldInputSuspended();'),
      (14,'llappviewer.cpp','ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()','ownsCamera()'),
      (15,'pipeline.cpp','mHighlightFaces.clear();','if(!ALCleanFrame::active()) mHighlightFaces.clear();'),
      (16,'llvopartgroup.cpp','if (!ALCleanParticleBatches::visible(effect,ALCleanFrame::active()) ||','if (false ||'),
      (17,'llappviewer.cpp','LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);',''),
      (18,'llviewermessage.cpp','ALKeyframePlayer::instance().targetRemoved(ALKeyframeViewer::id(id));',''),
      (19,'lllegacyatmospherics.cpp','if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FOG))','if (ALCleanFrame::active()) return;\n    if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FOG))'),
      (20,'pipeline.cpp','if (isTagRecordView() && !sReflectionRender && !sShadowRender)\n            ALKeyframePlayer::instance().drawGuides();','ALKeyframePlayer::instance().drawGuides();'),
      (21,'llviewerwindow.cpp','if(key==KEY_F4 || ALKeyframePlayer::instance().ownsCamera() || cast.isRunning()','if(key==KEY_F4 || cast.isRunning()'),
      (22,'llviewerwindow.cpp','(ALKeyframePlayer::instance().ownsCamera() || ALKeyframePlayer::instance().rlvHandbackActive()))'+chr(10)+'        { LLDirectorCast','(ALKeyframePlayer::instance().ownsCamera() && ALCleanFrame::active()))'+chr(10)+'        { LLDirectorCast'),
      (22,'llviewerwindow.cpp','(key==KEY_F3 || key==KEY_F4) &&','(key==KEY_F3) &&')]
    for rule,file,before,after in mutations:
        planted=dict(files)
        if rule==4:
            # Same-file relocation preserves all old file-level counts, but introduces
            # an unclassified owner and must fail the caller-identity census.
            file='alghoststudio.cpp'
            call='gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)'
            planted[file]=planted[file].replace(call,'true',1)+'\nbool unclassifiedCaller() { return '+call+'; }'

        elif rule==12:
            text=planted[file];start=text.index('{',text.index(before))+1;planted[file]=text[:start]+'if(ALCleanFrame::active()) return false;'+text[start:]
        elif rule==15:
            text=planted[file];start=text.index('void LLPipeline::renderHighlights()');planted[file]=text[:start]+text[start:].replace(before,after,1)
        else:
            assert before in planted[file],(rule,before);planted[file]=planted[file].replace(before,after,1)
        failed=assertions(planted)
        if rule==19:assert census(planted)==CENSUS, 'G2 must reproduce the original R4 blind spot'
        assert 'R'+str(rule) in failed,(rule,failed)
        print('PASS negative [src]: '+('authoritative_kill_provenance: ' if rule==18 else '')+'FAIL R'+str(rule))
    # R2-3: re-adding the shared main-view early return suppresses the existing Actor Mover
    # heading/path guides in snapshots with keyframes inactive; R20 must name that regression.
    planted=dict(files);text=planted['pipeline.cpp'];start=text.index('void LLPipeline::renderActorPathGuides(')
    start=text.index('{',start)+1
    planted['pipeline.cpp']=text[:start]+'\n    if (!isTagRecordView() || sReflectionRender || sShadowRender) return;'+text[start:]
    assert 'R20' in assertions(planted)
    print('PASS negative [src] feature_off_snapshot_guides_suppressed: FAIL R20')
    # C8 negative restores the old exact-restore correction and must fail R12.
    planted=dict(files);text=planted['llviewerwindow.cpp'];start=text.index('gDepthDirty = true;',text.index('bool LLViewerWindow::rawSnapshot('))
    before='if (!gPipeline.hasRawRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))'
    planted['llviewerwindow.cpp']=text[:start]+text[start:].replace(before,'if (prev_draw_ui != show_ui)',1)
    assert 'R12' in assertions(planted)
    print('PASS negative [src] raw_snapshot_exact_restore: FAIL R12')
    for name,mutate in [
        ('new_clean_file',lambda p:p.update({'__new_clean.cpp':'void guard(){if(ALCleanFrame::active()) return;}'})),
        ('new_clean_function',lambda p:p.update({'llviewerwindow.cpp':p['llviewerwindow.cpp']+'\nvoid newGuard(){if(ALCleanFrame::active()) return;}'})),
        ('multiline_clean_site',lambda p:p.update({'llviewerwindow.cpp':p['llviewerwindow.cpp']+'\nvoid newGuard(){if(ALCleanFrame::\nactive()) return;}'})),
        ('outside_newview_clean_site',lambda p:p.update({'../llrender/newguard.inl':'void guard(){if(ALCleanFrame::active()) return;}'})),
        ('overlay_bypasses_pass',lambda p:p.update({'alkeyframeoverlay.cpp':p['alkeyframeoverlay.cpp'].replace('!gPipeline.isTagRecordView()','false')}))]:
        planted=dict(files);mutate(planted);rule='R20' if name.startswith('overlay') else 'R19'
        assert rule in assertions(planted);print('PASS negative [src] '+name+': FAIL '+rule)
    assert not assertions(files)
    print('PASS clean runtime after 30 named source negatives')
if __name__=='__main__':check()
