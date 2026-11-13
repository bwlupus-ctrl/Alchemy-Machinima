/** Full GL transaction for the on-demand shaft probe (including allocation failure).
 * Include after the viewer's GL/render headers. Also compiled with strict fixture GL.
 */
#ifndef AL_SHAFT_GL_STATE_H
#define AL_SHAFT_GL_STATE_H
#include <vector>
namespace ALShaftSelfTest
{
class ScopedGLState
{
public:
    ScopedGLState()
    {
        gGL.flush();
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);
        glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        GLint count = 1;
        if (drawFBO) glGetIntegerv(GL_MAX_DRAW_BUFFERS, &count);
        drawBuffers.resize(count);
        for (GLint i = 0; i < count; ++i)
        {
            GLint buffer;
            glGetIntegerv(drawFBO ? GL_DRAW_BUFFER0 + i : GL_DRAW_BUFFER, &buffer);
            drawBuffers[i] = buffer;
        }
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
        glGetIntegerv(GL_PACK_SWAP_BYTES, &swapBytes);
    }
    ~ScopedGLState() { restore(); }
    ScopedGLState(const ScopedGLState&) = delete;
    ScopedGLState& operator=(const ScopedGLState&) = delete;
    // Explicit completion lets the verdict include errors from restoration itself.
    void restore()
    {
        if (restored) return;
        restored = true;
        gGL.flush();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
        if (!drawFBO) glDrawBuffer(drawBuffers[0]);
        else glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
        glReadBuffer(readBuffer);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        if (scissorEnabled) glEnable(GL_SCISSOR_TEST);
        else glDisable(GL_SCISSOR_TEST);
        gGL.setColorMask(mask[0], mask[1], mask[2], mask[3]);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, packBuffer);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
        glPixelStorei(GL_PACK_SWAP_BYTES, swapBytes);
        // Drain every error on every exit, and log in debug too (no pending error
        // can reach a later fatal GL check or escape behind a successful verdict).
        for (GLenum error; (error = glGetError()) != GL_NO_ERROR; )
        {
            failed = true;
            LL_WARNS("ShaftSelfTest") << "#ShaftSelfTest GL error at probe completion: " << error << LL_ENDL;
        }
    }
    bool failed = false;
private:
    bool restored = false;
    GLint drawFBO, readFBO, readBuffer, viewport[4], scissor[4];
    GLint packBuffer, alignment, rowLength, skipRows, skipPixels, swapBytes;
    GLboolean scissorEnabled, mask[4];
    std::vector<GLenum> drawBuffers; // on demand only; never constructed while idle
};
}
#endif
