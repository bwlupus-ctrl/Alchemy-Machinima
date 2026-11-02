/** Live-window-only POV eyelids. Runtime gestures never enter saved settings. */
#ifndef AL_POV_BLINK_H
#define AL_POV_BLINK_H
class LLRenderTarget;
class LLVertexBuffer;
class LLGLSLShader;
extern LLGLSLShader gPOVBlinkProgram;
namespace ALPOVBlink
{
void blink();
void setUIHeld(bool held);
bool uiHeld();
void setPadHeld(bool held);
// Single last scene-image write; never redirects the presented destination.
void renderLastSceneImage(bool live, LLRenderTarget* scratch, LLVertexBuffer* triangle);
void releaseGL();
}
#endif
