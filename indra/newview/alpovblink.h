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
// Only call for the admitted live main view. Returns null without GL work idle.
LLRenderTarget* beginPresentation(unsigned width, unsigned height, float& amount);
void endPresentation(LLVertexBuffer* triangle, float amount);
void releaseGL();
}
#endif
