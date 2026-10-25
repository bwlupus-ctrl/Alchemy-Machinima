"""Viewer geometry/settings services for the production WP7 owner and traversal.

All resource, scheduling, matrix, publication and sampler code is extracted from
the selected source ref. Native alpha-mask fragment policy runs unchanged on GL.
The bounded 256-object scene/culler is a fixture, not a native scene benchmark.
"""
import ctypes as C
from weather_compute_fixture import function
from weather_particles_fixture import build as particle_build


def build(tree, out, gl, mutation=None):
    def extension(prefix, methods, suffix, entries, sources):
        # Additional native material upload entrypoint and reserved uniforms.
        import re
        reserved = re.findall(r'mReservedUniforms.push_back\("([^"]+)"\)',
            function(tree.read('indra/llrender/llshadermgr.cpp'),'void LLShaderMgr::initAttribsAndUniforms('))
        names = ('ENABLED','VIEW_TO_AGENT','ORIGIN','WET','TIME_RANGE')
        slots = ','.join('WEATHER_MATERIAL_'+n+'='+str(reserved.index('weather_material_'+n.lower())) for n in names)
        prefix = prefix.replace('enum {DEFERRED_DEPTH=', 'enum {'+slots+',DEFERRED_DEPTH=')
        overloads = '''
 void uniform1i(int n,int v){glUniform1i(mUniform[n],v);}
 void uniform2f(int n,float a,float b){glUniform2f(mUniform[n],a,b);}
 void uniform3f(int n,float a,float b,float c){glUniform3f(mUniform[n],a,b,c);}
 void uniform4f(int n,float a,float b,float c,float d){glUniform4f(mUniform[n],a,b,c,d);}
'''
        prefix = prefix.replace(' bool isComplete()const{return mProgramObject!=0;}', overloads+' bool isComplete()const{return mProgramObject!=0;}')
        prefix = prefix.replace('struct LLRenderTarget {unsigned fbo=', 'extern bool forceAllocationFailure;\nstruct LLRenderTarget {unsigned fbo=')
        prefix = prefix.replace('  w=width;h=height;owned=true;', '  if(forceAllocationFailure)return false;\n  w=width;h=height;owned=true;')
        # Native LLTexUnit getters read viewer caches, not glGetIntegerv. Keep
        # those services faithful when timing 32-unit state transactions. The
        # fixture explicitly imports external Python GL bindings before work.
        prefix=prefix.replace('unsigned index=0;', 'unsigned index=0,cachedTexture=0;')
        prefix=prefix.replace('unsigned getCurrTexture(){int n;activate();glGetIntegerv(0x8069,&n);return n;}',
            'unsigned getCurrTexture(){return cachedTexture;}')
        prefix=prefix.replace('void bindManual(eTextureType,unsigned n,bool){activate();glBindTexture(0x0DE1,n);}',
            'void bindManual(eTextureType,unsigned n,bool){activate();if(cachedTexture!=n){glBindTexture(0x0DE1,n);cachedTexture=n;}}')
        begin = prefix.index(' void renderShadow(')
        finish = prefix.index('\n DECLARE_SHELTER',begin) if '\n DECLARE_SHELTER' in prefix[begin:] else -1
        original = function(prefix,' void renderShadow(')
        prefix = prefix.replace(original, ''' void renderShadow(const glm::mat4& v,const glm::mat4& p,LLCamera&,LLCullResult&,bool,bool,bool,bool,bool){sceneDraw(v,p);}''')
        # The scene implementation precedes the pipeline service declaration.
        prefix = prefix.replace('unsigned coverTraversals=0,coverBatches=0;GLuint casterProgram=0;',
            'unsigned coverTraversals=0,coverBatches=0;GLuint casterProgram=0;void sceneDraw(const glm::mat4&,const glm::mat4&);')
        methods += '\n'+function(tree.read('indra/newview/alweatherresources.cpp'),'void ALWeatherResources::bind(')
        methods += '\n'+API+'\n'+COST_API
        if 'beginTile' in tree.read('indra/newview/alweatherresources.h'):
            methods += r'''
extern "C" __declspec(dllexport) unsigned shelter_pending(){return fixturePipeline.mWeatherShelter.updates.pendingTiles();}
'''
        if 'mWeatherPrecipitationCoverCache.schedule(' in methods:
            methods=methods.replace('if(dirty)++fixturePipeline.mWeatherGeometryRevision;', '''
 if(dirty){
  fixturePipeline.dirtyWeatherRegion(LLVector3(oldRoofOffset-1,-5,0),LLVector3(oldRoofOffset+oldRoofWidth+1,5,128));
  fixturePipeline.dirtyWeatherRegion(LLVector3(roofOffset-1,-5,0),LLVector3(roofOffset+((sceneKind==1||sceneKind==4)?200.f:8.f)+1,5,128));
 }''')
        sync_entries=[
            ('glGetUniformiv','void','unsigned,int,int*',None,[C.c_uint,C.c_int,C.c_void_p]),
            ('glFenceSync','void*','unsigned,unsigned',C.c_void_p,[C.c_uint,C.c_uint]),
            ('glClientWaitSync','unsigned','void*,unsigned,unsigned long long',C.c_uint,[C.c_void_p,C.c_uint,C.c_uint64]),
            ('glDeleteSync','void','void*',None,[C.c_void_p]),
        ]
        prefix+='\n'+ '\n'.join(f'{ret} (*{name})({args});' for name,ret,args,_,_ in sync_entries)+'\n'
        methods+='\nunsigned record_listener_wait(void* fence,unsigned flags,unsigned long long timeout){++forbiddenListenerWaits;return glClientWaitSync(fence,flags,timeout);}\n'
        # Declaration must precede the mutated production caller.
        prefix+='unsigned record_listener_wait(void*,unsigned,unsigned long long);\n'
        inject='\n'.join(f'{name}=reinterpret_cast<decltype({name})>(p[{len(entries)+i}]);' for i,(name,*_) in enumerate(sync_entries))
        suffix=suffix.replace('LLShaderMgr::instance()->root=root;',inject+'\nLLShaderMgr::instance()->root=root;')
        if mutation == 'reverse-shear':
            # Mutate the compiled production source, not a fixture shadow formula.
            source = out/'alweathershelter.cpp'; text=source.read_text(encoding='utf-8')
            old='return {wind_x * scale, wind_y * scale};';assert old in text
            source.write_text(text.replace(old,'return {-wind_x * scale, -wind_y * scale};'),encoding='utf-8')
        changes = {
            'last-view': ('map.publish(projection * view,', 'map.publish(currentProjection * currentView,'),
            'forged-allocation': ('return mReady && mPublishedFrame == frame', 'return !mReady || mPublishedFrame == frame'),
            'tilted-height': ('shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, false, glm::value_ptr(mMatrix));',
                'shader->uniformMatrix4fv(LLShaderMgr::WEATHER_SURFACE_COVER_MATRIX, 1, false, glm::value_ptr(tilted ? advanced->layers[0].mMatrix : mMatrix));'),
            'publish-barrier': ('record_glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);', '(void)0;'),
            'mixed-generation': ('mPublishedFrame = frame;\n    updates.publish();','mPublishedFrame = frame-1;\n    updates.publish();'),
            'numeric-publication': ('if (!finite_map)', 'if (false)'),
            'blend-leak': ('if (mDraw[0].blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);', 'glEnable(GL_BLEND);'),
            'draw-buffer-leak': ('if (mFull) glDrawBuffers(mDrawFBO ? mBuffers : 1, buffers.data());', 'if (mFull) {const GLenum none=GL_NONE;glDrawBuffers(1,&none);}'),
            'moving-roof': ('mWeatherShelter.updates.dirty(world_low, world_high);', '(void)world_low; (void)world_high;'),
            'listener-wait': ('    mWeatherShelter.publish(gFrameCount);', '    mWeatherShelter.publish(gFrameCount); {auto fence=glFenceSync(0x9117,0);record_listener_wait(fence,0,0);glDeleteSync(fence);}'),
        }
        if mutation in changes:
            a,b=changes[mutation];assert a in methods, mutation;methods=methods.replace(a,b)
        return prefix, methods, suffix, entries+sync_entries, sources

    native = particle_build(tree,out,gl,extension=extension)
    shaderdir=out/'class1/deferred'
    for name in ('weatherAdvancedRainF','postDeferredNoTCV'):
        source=tree.read('indra/newview/app_settings/shaders/class1/deferred/'+name+'.glsl')
        if name=='weatherAdvancedRainF':
            util=tree.read('indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl')
            depth='uniform sampler2D depthMap;uniform mat4 inv_proj;\n'
            depth+='\n'.join(function(util,s) for s in ('vec2 getScreenCoordinate(','float getDepth(','vec4 getPosition('))
            source=depth+'\n'+source
        (shaderdir/(name+'.glsl')).write_text(source,encoding='utf-8')
    native.shelter_prepare.argtypes=[C.c_double]*4+[C.c_int]
    native.shelter_bind.argtypes=[C.c_uint,C.c_int]
    native.shelter_scene.argtypes=[C.c_int,C.c_float,C.c_float,C.c_float]
    native.shelter_origin.argtypes=[C.c_double]*3
    native.shelter_status.restype=C.c_char_p
    native.shelter_metrics.argtypes=[C.c_void_p]
    native.shelter_program.argtypes=[C.c_uint]
    return native


SCENE=r'''
int sceneKind=0;float roofOffset=0,roofHeight=4,roofAlpha=1;
float oldRoofOffset=0,oldRoofWidth=8;
int sceneMVP=-1,sceneRect=-1,sceneAlpha=-1,sceneZ=-1;
bool forceAllocationFailure=false;unsigned forbiddenListenerWaits=0;
U64 shelterTraversals=0,coverTriangles=0;unsigned sceneVertices=6;
void sceneDraw(const glm::mat4& view,const glm::mat4& projection){
 ++coverTraversals;
 glm::mat4 vp=projection*view;
 struct Batch {float x,y,width,height,z;int id;};
 std::array<Batch,256> batches;unsigned count=0;
 {
 WEATHER_FIXTURE_PASS("fixture cull and sort");
 for(int i=0;i<256;++i){
  // Known doorway/overhang plus 255 static objects, all culled against the
  // actual affine precipitation frustum. No per-map object allocations.
  float x=i?80.f+float(i%16)*3.f:roofOffset;
  float y=i?30.f+float(i/16)*3.f:-4.f;
  x-=float(gAgent.origin.mdV[0]);y-=float(gAgent.origin.mdV[1]);
  float sceneHeight=roofHeight-float(gAgent.origin.mdV[2]);
  if(i==0 && sceneKind==4)sceneHeight=-roofHeight;
  float width=i?2.f:((sceneKind==1 || sceneKind==4)?200.f:8.f), height=i?2.f:8.f;
  if(i==0 && sceneKind==2)height=-4.f;
  float cx=x+width*.5f,cy=y+height*.5f,cz=sceneHeight;
  float hx=std::abs(width)*.5f,hy=std::abs(height)*.5f,hz=0;
  if(height<0){cx=x;cy=y+width*.5f;cz=-height*.5f;hx=0;hy=width*.5f;hz=-height*.5f;}
  if(sceneHeight<0){cz=-sceneHeight+.5f*cx;hz=.5f*hx;}
  glm::vec4 c=vp*glm::vec4(cx,cy,cz,1);
  bool visible=true;
  for(int axis=0;axis<3;++axis){float radius=std::abs(vp[0][axis])*hx+std::abs(vp[1][axis])*hy+std::abs(vp[2][axis])*hz;
   if(std::abs(c[axis])>1+radius)visible=false;}
  if(!visible)continue;
  batches[count++]={x,y,width,height,sceneHeight,i};
 }
 // Render-map services are simulated; geometry and material membership remain
 // distinct, unlike the former repeated full-screen triangle submissions.
 std::sort(batches.begin(),batches.begin()+count,[](const Batch& a,const Batch& b){return a.id%8<b.id%8;});
 }
 WEATHER_FIXTURE_PASS("caster draw");
 glUseProgram(casterProgram);
 glUniformMatrix4fv(sceneMVP,1,0,glm::value_ptr(vp));glUniform1f(sceneAlpha,roofAlpha);
 for(unsigned i=0;i<count;++i){auto& batch=batches[i];
  glUniform1f(sceneAlpha,batch.id==0?roofAlpha:1.f);
  glUniform4f(sceneRect,batch.x,batch.y,batch.width,batch.height);
  // Test geometry is synthesized by the vertex service; native shadow alpha
  // cutoff and discard rules execute in its unchanged fragment module.
  glUniform1f(sceneZ,batch.z);
#ifdef LL_DRAW_BUDGET_H
  unsigned offset=0,vertices=sceneVertices;
  if(LLDrawBudget::take(0,offset,vertices)){glDrawArrays(GL_TRIANGLES,offset,vertices);++coverBatches;coverTriangles+=vertices/3;}
#else
  glDrawArrays(GL_TRIANGLES,0,6);++coverBatches;
#endif
 }
 glUseProgram(0);
}
'''

API=r'''
extern "C" __declspec(dllexport) void shelter_program(unsigned p){
 casterProgram=p;sceneMVP=glGetUniformLocation(p,"fixture_mvp");sceneRect=glGetUniformLocation(p,"fixture_rect");
 sceneAlpha=glGetUniformLocation(p,"fixture_alpha");sceneZ=glGetUniformLocation(p,"fixture_z");}
extern "C" __declspec(dllexport) void shelter_failure(int fail){forceAllocationFailure=fail!=0;}
extern "C" __declspec(dllexport) void shelter_sync(){
 int active;glGetIntegerv(0x84E0,&active);
 for(unsigned i=0;i<32;++i){glActiveTexture(0x84C0+i);int n;glGetIntegerv(0x8069,&n);gGL.units[i].cachedTexture=n;}
 glActiveTexture(active);
}
extern "C" __declspec(dllexport) void shelter_scene(int kind,float x,float z,float alpha){
 oldRoofOffset=roofOffset;oldRoofWidth=(sceneKind==1||sceneKind==4)?200.f:8.f;
 sceneKind=kind;roofOffset=x;roofHeight=z;roofAlpha=alpha;}
extern "C" __declspec(dllexport) void shelter_origin(double x,double y,double z){gAgent.origin={{x,y,z}};}
extern "C" __declspec(dllexport) void shelter_prepare(double time,double x,double y,double z,int dirty){
 static double previousTime=0;
 const int frames=std::clamp(int(std::floor(std::max(0.,time-previousTime)*60+.0001)),1,120);
 const double start=previousTime;previousTime=time;
 // Advance the real owners on the display frames between fixture timestamps.
 // Large functional-test time jumps must not hide amortised dirty work.
 for(int frame=1;frame<=frames;++frame){
  LLPresentationTime::seconds=start+(time-start)*frame/frames;
  ++gFrameCount;if(dirty)++fixturePipeline.mWeatherGeometryRevision;
  LLCamera camera;camera.eye={float(x),float(y),float(z)};
  fixturePipeline.generateWeatherSurfaceCover(camera);
  const U32 before=coverTraversals;fixturePipeline.generateWeatherAdvancedShelter(camera);
  shelterTraversals+=coverTraversals-before;
 }
}
extern "C" __declspec(dllexport) void shelter_bind(unsigned p,int kind){
 static std::map<unsigned,LLGLSLShader> programs;
 auto& shader=programs[p];shader.mProgramObject=p;if(shader.mUniform.empty())shader.mapUniforms();shader.bind();
 glm::dvec3 origin(gAgent.origin.mdV[0],gAgent.origin.mdV[1],gAgent.origin.mdV[2]);
 if(kind==0)fixturePipeline.mWeatherShelter.bind(&shader,origin,gFrameCount);
 if(kind==1)coverage.bindPrecipitation(&shader,origin,gFrameCount);
 if(kind==2)coverage.bind(&shader,glm::mat4(1),origin,gFrameCount);
 // Externally owned program; leave it bound for the fixture's consuming draw.
 shader.mProgramObject=0;LLGLSLShader::sCurBoundShaderPtr=nullptr;LLGLSLShader::sCurBoundShader=0;
}
extern "C" __declspec(dllexport) const char* shelter_status(){return fixturePipeline.mWeatherShelter.status().c_str();}
extern "C" __declspec(dllexport) U64 shelter_traversals(){return shelterTraversals;}
extern "C" __declspec(dllexport) unsigned shelter_layer(int layer){auto& a=fixturePipeline.mWeatherShelter.layers[layer];return a.mCover[a.mRead].texture;}
extern "C" __declspec(dllexport) unsigned shelter_rain_program(){return fixturePipeline.mWeatherShelter.rainProgram().mProgramObject;}
extern "C" __declspec(dllexport) int shelter_basic_allocated(){return coverage.allocated();}
extern "C" __declspec(dllexport) void shelter_depth(unsigned unit,unsigned texture){gGL.getTexUnit(unit)->bindManual(LLTexUnit::TT_TEXTURE,texture,false);}
extern "C" __declspec(dllexport) void shelter_rain_state(float* out){
 auto& weather=ALWeatherBridge::Controller::instance();auto rain=ALWeatherBridge::rain(weather.state(),.55f,.65f);
 out[0]=rain.windX;out[1]=rain.windY;out[2]=float(std::fmod(weather.clock().seconds(),4096.));
}
extern "C" __declspec(dllexport) void shelter_invalid_publish(){
 auto& owner=fixturePipeline.mWeatherShelter;
 glm::mat4 invalid(1);invalid[0][0]=std::nanf("");
 owner.layers[0].mPreparedFrame=gFrameCount;
 owner.layers[0].publish(invalid,128,0,true,owner.layers[0].mOrigin,ALWeatherBridge::Controller::instance().placeSerial());
 owner.publish(gFrameCount);
}
extern "C" __declspec(dllexport) void shelter_metrics(double* out){
 auto& a=fixturePipeline.mWeatherShelter;auto& s=ALWeatherBridge::Controller::instance().settings();
 out[0]=coverTraversals;out[1]=coverBatches;out[2]=a.current(glm::dvec3(gAgent.origin.mdV[0],gAgent.origin.mdV[1],gAgent.origin.mdV[2]),gFrameCount);
 out[3]=a.layers[0].mCover[0].getWidth();out[4]=a.layers[1].mCover[0].getWidth();
 out[5]=ALWeatherShelter::advancedBytes(U32(out[3]),U32(out[4]));
 out[6]=barriers;out[7]=forbiddenListenerWaits;
 out[8]=s.shelterHz;out[9]=s.shelterNearExtent;out[10]=s.shelterFarExtent;
 out[11]=programCreates;out[12]=queries;out[13]=hostAllocations;
}
'''

# Only the scene/view dispatch is a fixture. Keep it compiled so Python/ctypes
# scheduling gaps are not counted as GPU work between the production passes.
# Resource preparation, traversals, weather idle, bindings and particle render
# above remain the extracted production implementations. Rain constants come
# from the separately extracted native uniform-upload adapter.
COST_API=r'''
extern "C" void fixture_weather_set(const char*,double);
extern "C" void fixture_weather_step(double);
struct FixtureShelterCost {
 unsigned fbo=0,depth=0,vao=0,material=0,programs[2]{},textures[3]{};
 int w=0,h=0,locations[2][16]{},depthUnits[2]{},materialLine=-1;
 float values[13][4]{};glm::mat4 projection,inverseProjection,views[2],inverseViews[2],identity{1.f};
 void target(){
  glBindFramebuffer(0x8D40,fbo);
  const unsigned buffers[3]={0x8CE0,0x8CE1,0x8CE2};
  for(unsigned i=0;i<3;++i)glFramebufferTexture2D(0x8D40,buffers[i],0x0DE1,textures[i],0);
  glDrawBuffers(3,buffers);glViewport(0,0,w,h);
  glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);
 }
 void rain(bool on,unsigned view){
  WEATHER_FIXTURE_PASS("weather rain consumer");
  target();shelter_bind(programs[on],0);
  shelter_depth(depthUnits[on],depth);
  glUniformMatrix4fv(locations[on][14],1,0,glm::value_ptr(inverseProjection));
  glUniformMatrix4fv(locations[on][15],1,0,glm::value_ptr(inverseViews[view]));
  float state[3];shelter_rain_state(state);
  for(unsigned i=0;i<13;++i){
   const int location=locations[on][i];const float* value=values[i];
   if(i==0)glUniform3f(location,state[0],state[1],0);
   else if(i==1)glUniform3f(location,value[0],value[1],value[2]);
   else if(i>=11)glUniform1i(location,int(value[0]));
   else glUniform1f(location,i==2?state[2]:value[0]);
  }
  const unsigned beauty=0x8CE0;glDrawBuffers(1,&beauty);
  glViewport(0,0,view?w:w/2,view?h:h/2);
  glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);glBindVertexArray(0);
 }
 void consumers(bool on,unsigned view){
  target();const double origin[3]{};
  render(fbo,depth,0,w,h,glm::value_ptr(views[view]),glm::value_ptr(projection),glm::value_ptr(identity),origin,-1);
  {
   WEATHER_FIXTURE_PASS("weather material consumer");
   target();shelter_bind(material,2);
   glUniform4f(materialLine,view?20.f:0.f,view?84.f:64.f,0,0);
   const unsigned beauty=0x8CE0;glDrawBuffers(1,&beauty);glDrawArrays(GL_TRIANGLES,0,3);
  }
  rain(on,view);
 }
} fixtureShelterCost;
extern "C" __declspec(dllexport) void shelter_cost_setup(const unsigned* objects,const int* locations,const float* values,
 const float* projection,const float* inverseProjection,const float* views,const float* inverseViews,int w,int h){
 auto& c=fixtureShelterCost;c.fbo=objects[0];c.depth=objects[1];c.vao=objects[2];c.material=objects[3];
 c.programs[0]=objects[4];c.programs[1]=objects[5];std::memcpy(c.textures,objects+6,sizeof(c.textures));
 c.w=w;c.h=h;std::memcpy(c.locations,locations,sizeof(c.locations));std::memcpy(c.values,values,sizeof(c.values));
 std::memcpy(&c.projection,projection,64);std::memcpy(&c.inverseProjection,inverseProjection,64);
 std::memcpy(c.views,views,128);std::memcpy(c.inverseViews,inverseViews,128);
 c.materialLine=glGetUniformLocation(c.material,"sample_line");
 for(unsigned i=0;i<2;++i){shelter_bind(c.programs[i],0);glGetUniformiv(c.programs[i],c.locations[i][13],&c.depthUnits[i]);}
}
extern "C" __declspec(dllexport) void shelter_cost_refresh(int on,int tick){
 const double t=10+double(tick)/60;
 // The paired immutable baseline does not share Python's interleaved toggle.
 // Each native measurement must explicitly select its own production owner.
 fixture_weather_set("AlchemyWeatherAdvancedShelterEnabled",on?1:0);
 shelter_scene(0,float(tick%7)*.01f,4,1);
 fixture_weather_set("AlchemyWeatherWindDirection",270+double(tick%5)*.1);
 fixture_weather_step(t);shelter_prepare(t,double(tick%16)*.01,0,0,1);
}
extern "C" __declspec(dllexport) void shelter_cost_frame(int on,int dual,int tick){
 shelter_cost_refresh(on,tick);
 fixtureShelterCost.consumers(on!=0,0);
 if(dual)fixtureShelterCost.consumers(on!=0,1);
}
extern "C" __declspec(dllexport) void shelter_cost_rain(int on,int view){fixtureShelterCost.rain(on!=0,view);}
'''
