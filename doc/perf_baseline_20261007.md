# Perf baseline 2026-10-07 (Tracy, Release + GPU zones)

## GPU per frame - ReShade ON (~46 fps, 1396 frames/30 s)

| zone | ms/frame | calls/frame | file |
|---|---|---|---|
| doFrame | 21.625 | 1 | llappviewer.cpp |
| Render | 20.453 | 1 | llviewerdisplay.cpp |
| swap | 12.202 | 1 | llviewerdisplay.cpp |
| renderDeferredLighting | 4.289 | 1.2 | pipeline.cpp |
| generateSunShadow | 3.191 | 1.2 | pipeline.cpp |
| renderShadow | 3.115 | 7.7 | pipeline.cpp |
| shadow simple | 2.219 | 15.4 | pipeline.cpp |
| renderGeomPostDeferred | 1.494 | 1.2 | pipeline.cpp |
| LLRenderTarget::copyContents | 1.476 | 6.47 | llrendertarget.cpp |
| reflection manager update | 1.187 | 1 | llreflectionmapmanager.cpp |
| probe update | 1.187 | 0.2 | llreflectionmapmanager.cpp |
| cubeSnapshot | 1.168 | 0.2 | llviewerwindow.cpp |
| display cube face | 1.167 | 0.2 | llviewerdisplay.cpp |
| renderGeomDeferred | 1.028 | 1.2 | pipeline.cpp |
| ui | 0.821 | 1 | llviewerdisplay.cpp |
| renderFinalize | 0.784 | 1 | pipeline.cpp |
| shadow alpha | 0.74 | 7.7 | pipeline.cpp |
| sun program | 0.668 | 1.2 | pipeline.cpp |
| shadow alpha masked | 0.612 | 30.8 | pipeline.cpp |
| fullscreen lights | 0.552 | 1.2 | pipeline.cpp |
| soften shadow | 0.493 | 1 | pipeline.cpp |
| atmospherics | 0.475 | 1.2 | pipeline.cpp |
| on-lens filters | 0.264 | 1 | pipeline.cpp |
| clear | 0.212 | 24.82 | llrendertarget.cpp |
| bloom hdr generate | 0.179 | 1 | pipeline.cpp |
| renderGeomVelocity | 0.152 | 1 | pipeline.cpp |
| stateSort | 0.12 | 8.9 | pipeline.cpp |
| shadow alpha grass | 0.103 | 15.4 | pipeline.cpp |
| rebuildMesh | 0.101 | 8.9 | pipeline.cpp |
| shadow alpha material | 0.097 | 15.4 | pipeline.cpp |
| renderFinalize - final blit | 0.093 | 1 | pipeline.cpp |
| LLRenderTarget::copyContentsFromAttachment | 0.088 | 1 | llrendertarget.cpp |
| SMAA Edge | 0.072 | 1 | pipeline.cpp |
| rt flush | 0.07 | 39.06 | llrendertarget.cpp |
| haze | 0.068 | 2.4 | pipeline.cpp |
| generate mip map | 0.068 | 0.07 | llimagegl.cpp |
| createGLTexture | 0.068 | 0.07 | llimagegl.cpp |
| colorcorrect | 0.063 | 1 | pipeline.cpp |
| SMAA | 0.055 | 1 | pipeline.cpp |
| doOcclusion | 0.052 | 1 | pipeline.cpp |
| luminance sample | 0.039 | 1 | pipeline.cpp |
| bindTarget | 0.033 | 40.26 | llrendertarget.cpp |
| rt generate mipmaps | 0.03 | 1 | llrendertarget.cpp |
| renderGeomShadow | 0.021 | 7.7 | pipeline.cpp |
| projectors | 0.012 | 0.9 | pipeline.cpp |
| probe mip | 0.011 | 1.4 | llreflectionmapmanager.cpp |
| shadow alpha blend | 0.01 | 15.4 | pipeline.cpp |
| exposure sample | 0.01 | 1 | pipeline.cpp |

## GPU per frame - ReShade effects OFF (VSync-capped 60 fps, 914 frames/15 s)

| zone | ms/frame | calls/frame | file |
|---|---|---|---|
| doFrame | 16.666 | 1 | llappviewer.cpp |
| Render | 15.475 | 1 | llviewerdisplay.cpp |
| swap | 7.792 | 1 | llviewerdisplay.cpp |
| renderDeferredLighting | 4.352 | 1.19 | pipeline.cpp |
| generateSunShadow | 2.617 | 1.19 | pipeline.cpp |
| renderShadow | 2.543 | 7.02 | pipeline.cpp |
| shadow simple | 1.794 | 14.03 | pipeline.cpp |
| renderGeomPostDeferred | 1.5 | 1.19 | pipeline.cpp |
| LLRenderTarget::copyContents | 1.475 | 6.38 | llrendertarget.cpp |
| probe update | 1.205 | 0.19 | llreflectionmapmanager.cpp |
| reflection manager update | 1.205 | 1 | llreflectionmapmanager.cpp |
| cubeSnapshot | 1.188 | 0.19 | llviewerwindow.cpp |
| display cube face | 1.187 | 0.19 | llviewerdisplay.cpp |
| renderGeomDeferred | 1.096 | 1.19 | pipeline.cpp |
| ui | 0.785 | 1 | llviewerdisplay.cpp |
| renderFinalize | 0.761 | 1 | pipeline.cpp |
| sun program | 0.707 | 1.19 | pipeline.cpp |
| shadow alpha | 0.65 | 7.02 | pipeline.cpp |
| fullscreen lights | 0.574 | 1.19 | pipeline.cpp |
| shadow alpha masked | 0.57 | 28.06 | pipeline.cpp |
| atmospherics | 0.487 | 1.19 | pipeline.cpp |
| soften shadow | 0.48 | 1 | pipeline.cpp |
| on-lens filters | 0.249 | 1 | pipeline.cpp |
| clear | 0.205 | 24.78 | llrendertarget.cpp |
| bloom hdr generate | 0.176 | 1 | pipeline.cpp |
| renderGeomVelocity | 0.159 | 1 | pipeline.cpp |
| renderFinalize - final blit | 0.093 | 1 | pipeline.cpp |
| LLRenderTarget::copyContentsFromAttachment | 0.088 | 1 | llrendertarget.cpp |
| SMAA Edge | 0.071 | 1 | pipeline.cpp |
| haze | 0.069 | 2.38 | pipeline.cpp |
| colorcorrect | 0.062 | 1 | pipeline.cpp |
| rt flush | 0.06 | 38.88 | llrendertarget.cpp |
| stateSort | 0.06 | 8.2 | pipeline.cpp |
| shadow alpha grass | 0.06 | 14.03 | pipeline.cpp |
| SMAA | 0.054 | 1 | pipeline.cpp |
| shadow alpha material | 0.054 | 14.03 | pipeline.cpp |
| doOcclusion | 0.049 | 1 | pipeline.cpp |
| luminance sample | 0.039 | 1 | pipeline.cpp |
| rebuildMesh | 0.038 | 8.2 | pipeline.cpp |
| bindTarget | 0.033 | 40.07 | llrendertarget.cpp |
| rt generate mipmaps | 0.029 | 1 | llrendertarget.cpp |
| renderGeomShadow | 0.024 | 7.02 | pipeline.cpp |
| exposure sample | 0.01 | 1 | pipeline.cpp |
| probe mip | 0.01 | 1.31 | llreflectionmapmanager.cpp |

## CPU top zones per frame - ReShade ON trace

| zone | ms/frame | calls/frame | file |
|---|---|---|---|
| LLAppViewer::doFrame | 21.619 | 1 | llappviewer.cpp:1454 |
| df Display | 18.964 | 1 | llappviewer.cpp:1618 |
| Render | 16.322 | 1 | llviewerdisplay.cpp:432 |
| display - 1 | 16.287 | 1 | llviewerdisplay.cpp:755 |
| display - 2 | 8.331 | 1 | llviewerdisplay.cpp:826 |
| LLPipeline::generateSunShadow | 8.319 | 1.2 | pipeline.cpp:28065 |
| ImageRequest::processRequest | 4.159 | 0.05 | llimageworker.cpp:159 |
| LLImageJ2C::decodeChannels | 4.151 | 0.06 | llimagej2c.cpp:142 |
| LLImageJ2C::decode | 4.094 | 0.05 | llimagej2c.cpp:134 |
| LLDiskCache::purge | 3.658 | 0 | lldiskcache.cpp:114 |
| Swap | 2.858 | 1 | llviewerdisplay.cpp:1835 |
| df Snapshot | 2.641 | 1 | llappviewer.cpp:1626 |
| LLReflectionMapManager::update | 2.638 | 1 | llreflectionmapmanager.cpp:284 |
| LLReflectionMapManager::doProbeUpdate | 2.49 | 0.2 | llreflectionmapmanager.cpp:2737 |
| LLReflectionMapManager::updateProbeFace | 2.49 | 0.2 | llreflectionmapmanager.cpp:2823 |
| LLReflectionMap::update | 2.432 | 0.2 | llreflectionmap.cpp:54 |
| LLViewerWindow::cubeSnapshot | 2.432 | 0.2 | llviewerwindow.cpp:6073 |
| df idle | 2.329 | 1 | llappviewer.cpp:1587 |
| LLAppViewer::idle | 2.328 | 1 | llappviewer.cpp:5165 |
| LLWindowWin32::swapBuffers | 2.233 | 1 | llwindowwin32.cpp:3928 |
| LLPipeline::renderDeferredLighting | 2.225 | 1.2 | pipeline.cpp:24848 |
| processRequest - retry | 2.002 | 1.07 | llqueuedthread.cpp:494 |
| render_ui | 1.751 | 1 | llviewerdisplay.cpp:1724 |
| HUD | 0.973 | 1 | llviewerdisplay.cpp:1757 |
| UI 2D | 0.95 | 1 | llviewerdisplay.cpp:1815 |
| LLPipeline::renderGeomPostDeferred | 0.916 | 1.2 | pipeline.cpp:7844 |
| render_ui_2d | 0.901 | 1 | llviewerdisplay.cpp:1980 |
| LLPipeline::renderShadow | 0.824 | 7.7 | pipeline.cpp:27426 |
| LLPipeline::renderFinalize | 0.766 | 1 | pipeline.cpp:22349 |
| LLFolderView::update | 0.704 | 8.03 | llfolderview.cpp:1725 |
| LLFolderView::filter | 0.686 | 8.03 | llfolderview.cpp:348 |
| LLFloater::draw | 0.648 | 6.16 | llfloater.cpp:2178 |
| LLViewerObjectList::update | 0.601 | 1 | llviewerobjectlist.cpp:917 |
| LLVOAvatar::idleUpdate | 0.563 | 2.84 | llvoavatar.cpp:2776 |
| deferred | 0.474 | 1.2 | pipeline.cpp:24986 |
| LLPipeline::renderActorMask | 0.445 | 1 | pipeline.cpp:24614 |
| LLPipeline::renderGeomDeferred | 0.428 | 1.2 | pipeline.cpp:7533 |
| deferred poolrender | 0.408 | 8.8 | pipeline.cpp:8057 |
| deferred pools | 0.407 | 1.2 | pipeline.cpp:7567 |
| display - 5 | 0.399 | 1 | llviewerdisplay.cpp:1043 |
| LLPipeline::stateSort | 0.341 | 8.9 | pipeline.cpp:4748 |
| Update Images | 0.337 | 1 | llviewerdisplay.cpp:889 |
| LLDrawPoolAlpha::renderPostDeferred | 0.326 | 2.21 | lldrawpoolalpha.cpp:163 |
| deferred pool render | 0.324 | 11.13 | pipeline.cpp:7618 |
| List | 0.323 | 1 | llviewerdisplay.cpp:902 |
| LLViewerTextureList::updateImages | 0.322 | 1 | llviewertexturelist.cpp:817 |
| LLTextBase::draw | 0.322 | 41.65 | lltextbase.cpp:1541 |
| LLViewerWindow::updateUI | 0.295 | 1 | llviewerwindow.cpp:3895 |
| LLGLSLShader::bind | 0.293 | 607.47 | llglslshader.cpp:1079 |
| LLEnvironment::updateShaderUniforms | 0.244 | 123.16 | llenvironment.cpp:1740 |
| LLWindowWin32::mainWindowProc | 0.238 | 20.82 | llwindowwin32.cpp:2485 |
| Render Cube Face | 0.238 | 0.2 | llviewerdisplay.cpp:1356 |
| LLShaderUniforms::apply | 0.236 | 462.79 | llglslshader.cpp:2056 |
| df processMiscNativeEvents | 0.233 | 1 | llappviewer.cpp:1496 |
| shadow simple | 0.222 | 15.4 | pipeline.cpp:27568 |
| shadow alpha | 0.216 | 7.7 | pipeline.cpp:27594 |
| LLTextureCacheWorker::doWork | 0.214 | 0.15 | lltexturecache.cpp:945 |
| LLPipeline::doAtmospherics | 0.213 | 1.2 | pipeline.cpp:26316 |
| LLTextureCacheRemoteWorker::doWrite | 0.211 | 0.09 | lltexturecache.cpp:719 |
| LLViewerTextureList::updateImagesFetchTextures | 0.208 | 1 | llviewertexturelist.cpp:1250 |
