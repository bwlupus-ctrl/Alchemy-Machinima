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
def census(files):
    result={}
    token=re.compile(r'\b(?:hasRenderDebugFeatureMask|hasRawRenderDebugFeatureMask|getIndicatorsVisible)\s*\(|\bRENDER_DEBUG_FEATURE_UI\b')
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
    need(12,'if (toggled_ui)' in raw and raw.count('if (toggled_ui)')==2 and 'if(ALCleanFrame::active())' in raw and 'if(ALCleanFrame::active())' in simple and 'if(ALCleanFrame::active())' not in cube and all('hasRenderDebugFeatureMask(' not in b for b in (raw,simple,cube)) and 'if (prev_draw_ui)' in simple and 'if (prev_draw_ui)' in cube)
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
    return errors

def check():
    files={str(p.relative_to(NV)).replace('\\','/'):code(p.read_text(encoding='utf-8')) for p in NV.rglob('*') if p.suffix in ('.cpp','.h')}
    failures=assertions(files);assert not failures,failures
    print('PASS R1-R18 runtime; table-A census '+str(len(CENSUS))+' classified caller statements')
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
      (18,'llviewermessage.cpp','ALKeyframePlayer::instance().targetRemoved(ALKeyframeViewer::id(id));','')]
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
        assert 'R'+str(rule) in failed,(rule,failed)
        print('PASS negative [src]: '+('authoritative_kill_provenance: ' if rule==18 else '')+'FAIL R'+str(rule))
    assert not assertions(files)
    print('PASS clean runtime after 18 named source negatives')
if __name__=='__main__':check()
