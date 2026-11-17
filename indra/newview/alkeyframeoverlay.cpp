#include "llviewerprecompiledheaders.h"
#include "alkeyframeoverlay.h"
#include "alkeyframeplayer.h"
#include "alcleanframe.h"
#include "llagent.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llvertexbuffer.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llviewerdisplay.h"
#include "pipeline.h"
#include <cmath>

extern bool gSnapshot;
extern bool gCubeSnapshot;

namespace
{
LLPointer<LLVertexBuffer> sBuffer;
std::vector<ALKeyframeOverlay::Pick> sPicks;
ALKf::Vec3 sOrigin;
S32 sCount=0, sSelectedStart=0;
GLuint sVao=0;
}
const std::vector<ALKeyframeOverlay::Pick>& ALKeyframeOverlay::picks() {return sPicks;}
void ALKeyframeOverlay::clear()
{if(sVao) {glDeleteVertexArrays(1,&sVao);sVao=0;} sBuffer=nullptr; sCount=0; sPicks.clear();}
void ALKeyframeOverlay::refresh(const ALKf::Track& t,const ALKf::Cache& c,unsigned selected,bool dots,bool icons)
{
    clear(); if(t.keys.empty()||c.position<0) return;
    sOrigin=std::get<ALKf::Vec3>(t.keys.front().values[c.position]);
    std::vector<ALKf::Vec3> vertices, highlighted;
    auto line=[&](ALKf::Vec3 a,ALKf::Vec3 b,bool hi=false)
    {auto& v=hi?highlighted:vertices; v.push_back(a-sOrigin); v.push_back(b-sOrigin);};
    auto cross=[&](ALKf::Vec3 p,double size,bool hi)
    {line(p-ALKf::Vec3{size,0,0},p+ALKf::Vec3{size,0,0},hi); line(p-ALKf::Vec3{0,size,0},p+ALKf::Vec3{0,size,0},hi); line(p-ALKf::Vec3{0,0,size},p+ALKf::Vec3{0,0,size},hi);};
    const auto& curves=c.channels[c.position].spatial;
    for(const auto& arc:curves) for(size_t j=1;j<arc.table.size();++j) line(arc.curve.at(arc.table[j-1].u),arc.curve.at(arc.table[j].u));
    if(dots) for(double second=0;second<=std::min(c.end,100000.);second+=1)
        cross(std::get<ALKf::Vec3>(ALKf::evalChannel(t,c,c.position,second)),.035,false);
    for(size_t i=0;i<t.keys.size();++i)
    {
        const auto& k=t.keys[i]; const auto p=std::get<ALKf::Vec3>(k.values[c.position]);
        sPicks.push_back({k.id,0,p});
        if(icons)
        {
            const auto q=std::get<ALKf::Quat>(k.values[c.rotation]);
            const auto tip=p+q.rotate({.35,0,0});
            ALKf::Vec3 corners[4];
            for(int a=0;a<4;++a) corners[a]=p+q.rotate({0,(a&1)?.12:-.12,(a&2)?.08:-.08});
            for(int a=0;a<4;++a) {line(corners[a],tip,k.id==selected); line(corners[a],corners[a^1],k.id==selected); line(corners[a],corners[a^2],k.id==selected);}
        }
        if(k.id==selected)
        {
            cross(p,.08,true);
            if(i>0) {auto h=curves[i-1].curve.p[2]; line(p,h,true); cross(h,.06,true); sPicks.push_back({k.id,-1,h});}
            if(i<curves.size()) {auto h=curves[i].curve.p[1]; line(p,h,true); cross(h,.06,true); sPicks.push_back({k.id,1,h});}
        }
    }
    sSelectedStart=static_cast<S32>(vertices.size());
    vertices.insert(vertices.end(),highlighted.begin(),highlighted.end());
    sCount=static_cast<S32>(vertices.size()); if(!sCount) return;
    glGenVertexArrays(1,&sVao);
    sBuffer=new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
    sBuffer->allocateBuffer(sCount,0);
    LLStrider<LLVector3> out; sBuffer->getVertexStrider(out);
    for(S32 i=0;i<sCount;++i) out[i].set(static_cast<F32>(vertices[i].x),static_cast<F32>(vertices[i].y),static_cast<F32>(vertices[i].z));
    sBuffer->unmapBuffer();
}
void ALKeyframeOverlay::draw()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI;
    // Table A row 7, WP6: no GL or allocation before all recording/pass guards.
    if(!sBuffer || ALCleanFrame::active() || ALKeyframePlayer::instance().rlvHandbackActive() ||
       gSnapshot || gCubeSnapshot || LLPipeline::sPrismLensRender || LLPipeline::sReflectionRender ||
       !gViewerWindow || !gViewerWindow->getUIVisibility() ||
       !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI)) return;
    gGL.flush();
    auto* shader=LLGLSLShader::sCurBoundShaderPtr;
    const U32 cachedBuffer=LLVertexBuffer::sGLRenderBuffer, cachedIndices=LLVertexBuffer::sGLRenderIndices, cachedMask=LLVertexBuffer::sLastMask, cachedCount=LLVertexBuffer::sVertexCount;
    GLint vao=0, array=0, element=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao); glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array); glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&element);
    const GLint colorLocation=gDebugProgram.getUniformLocation(LLShaderMgr::DIFFUSE_COLOR);
    GLfloat color[4]; glGetUniformfv(gDebugProgram.mProgramObject,colorLocation,color);
    LLGLDepthTest depth(GL_TRUE,GL_FALSE,GL_LEQUAL);
    LLGLDisable blend(GL_BLEND);
    // Shader bind also changes vertex attribute enables: do it inside our VAO.
    glBindVertexArray(sVao);
    LLVertexBuffer::sGLRenderBuffer=0; LLVertexBuffer::sGLRenderIndices=0; LLVertexBuffer::sLastMask=0;
    gDebugProgram.bind();
    gGL.pushMatrix();
    const auto origin=gAgent.getPosAgentFromGlobal(LLVector3d(sOrigin.x,sOrigin.y,sOrigin.z));
    gGL.translatef(origin.mV[0],origin.mV[1],origin.mV[2]);
    // A private VAO isolates both attribute pointers and enable masks from beauty rendering.
    glBindVertexArray(sVao);
    LLVertexBuffer::sGLRenderBuffer=0; LLVertexBuffer::sGLRenderIndices=0; LLVertexBuffer::sLastMask=0;
    sBuffer->setBuffer();
    gDebugProgram.uniform4f(LLShaderMgr::DIFFUSE_COLOR,.2f,.8f,1.f,1.f);
    if(sSelectedStart) sBuffer->drawArrays(LLRender::LINES,0,sSelectedStart);
    gDebugProgram.uniform4f(LLShaderMgr::DIFFUSE_COLOR,1.f,.8f,.1f,1.f);
    if(sCount>sSelectedStart) sBuffer->drawArrays(LLRender::LINES,sSelectedStart,sCount-sSelectedStart);
    gGL.popMatrix();
    LLVertexBuffer::unbind();
    gDebugProgram.uniform4fv(LLShaderMgr::DIFFUSE_COLOR,1,color);
    // Restore the shader while still isolated; bind() can unbind buffers and
    // change attribute enables. Restore the original VAO and caches last.
    if(shader) shader->bind(); else LLGLSLShader::unbind();
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,array); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,element);
    LLVertexBuffer::sGLRenderBuffer=cachedBuffer; LLVertexBuffer::sGLRenderIndices=cachedIndices;
    LLVertexBuffer::sLastMask=cachedMask; LLVertexBuffer::sVertexCount=cachedCount;
}
