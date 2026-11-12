/** Explicit, viewer-local shaft probe. No timer or GPU work while idle. */
#ifndef AL_SHAFT_SELF_TEST_H
#define AL_SHAFT_SELF_TEST_H
#include <string>
class LLView;
class LLVOVolume;
class LLGLSLShader;
class LLRenderTarget;
namespace ALShaftSelfTest
{
extern bool active;
void start(LLView* owner);
void cancel(const char* reason);
std::string result();
// Called by the main-view caller after a pass with an eligible native slot owner.
void submitted(LLVOVolume*, LLGLSLShader&, LLRenderTarget&);
}
#endif
