#include "llviewerprecompiledheaders.h"
#include "allightrigmap.h"
#include "alcinelightrig.h"
#include "alcinelightrigmanager.h"
#include "alkitstyle.h"
#include "alkitpage.h"
#include "alkitshell.h"
#include "aluirefresh.h"
#include "llcallbacklist.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "llfontgl.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
static LLDefaultChildRegistry::Register<ALLightRigMap> r("light_rig_map");

namespace
{

    const char*ROLE[]=
    {
        "Key","Fill","Rim","Bg"
    }
    ;
    const std::string LABEL[]=
    {
        "KEY","FILL","BACK","BG"
    }
    ;

    const std::string S_OFF="OFF",S_CAM="CAM",S_BACK="BACK",S_CAM_L="CAM L",S_CAM_R="CAM R";
    const std::string S_SIDE_L="From cam L  \xE2\x87\x84",S_SIDE_R="From cam R  \xE2\x87\x84";
    const std::string S_SIDE_KEY="near / far / off",S_OTHER_KEY="in front / behind / off";

    void dashed(F32 x0,F32 y0,F32 x1,F32 y1,F32 dash,F32 gap,const LLColor4&c)
    {
        const F32 dx=x1-x0,dy=y1-y0,len=hypotf(dx,dy);
        if(len<.01f)return;
        const F32 ux=dx/len,uy=dy/len;
        for(F32 d=0.f;d<len;d+=dash+gap)gl_line_2d(ll_round(x0+ux*d),ll_round(y0+uy*d),ll_round(x0+ux*llmin(d+dash,len)),ll_round(y0+uy*llmin(d+dash,len)),c);
    }

    void ellipse(F32 cx,F32 cy,F32 rx,F32 ry,const LLColor4&c)
    {
        for(S32 i=0;i<48;++i)
        {
            const F32 a0=F_TWO_PI*i/48.f,a1=F_TWO_PI*(i+1)/48.f;
            gl_line_2d(ll_round(cx+rx*cosf(a0)),ll_round(cy+ry*sinf(a0)),ll_round(cx+rx*cosf(a1)),ll_round(cy+ry*sinf(a1)),c);
        }
    }
    void nearEllipse(F32 cx,F32 cy,F32 rx,F32 ry,const LLColor4&c)
    {
        for(S32 i=0;i<24;++i)
        {
            const F32 a0=-F_PI_BY_TWO+F_PI*i/24.f;
            const F32 a1=-F_PI_BY_TWO+F_PI*(i+1)/24.f;
            gl_line_2d(ll_round(cx+rx*sinf(a0)),ll_round(cy-ry*cosf(a0)),
                       ll_round(cx+rx*sinf(a1)),ll_round(cy-ry*cosf(a1)),c);
        }
    }

    void dashedCircle(F32 cx,F32 cy,F32 r,const LLColor4&c)
    {
        for(S32 i=0;i<24;i+=2)
        {
            const F32 a0=F_TWO_PI*i/24.f,a1=F_TWO_PI*(i+1)/24.f;
            gl_line_2d(ll_round(cx+r*cosf(a0)),ll_round(cy+r*sinf(a0)),ll_round(cx+r*cosf(a1)),ll_round(cy+r*sinf(a1)),c);
        }
    }

    void label(const std::string&s,F32 x,F32 y,const LLColor4&c,LLFontGL::HAlign a=LLFontGL::HCENTER)
    {
        if(const LLFontGL*f=LLFontGL::getFontSansSerifSmall())f->renderUTF8(s,0,ll_round(x),ll_round(y),c,a,LLFontGL::VCENTER);
    }

}

ALLightRigMap::Params::Params()
:   selector_card("selector_card", "kc_lights"),
    view_setting("view_setting", "DirectorV2LightMapView"),
    side_from_setting("side_from_setting", "DirectorV2LightMapSideFrom")
{
}

ALLightRigMap::ALLightRigMap(const Params&p):LLUICtrl(p),mSelectorCard(p.selector_card),mViewSetting(p.view_setting),mSideSetting(p.side_from_setting)
{

    setTabStop(true);
    mConnections.reserve(24);
    ALKitStyle::color(ALKitStyle::WINDOW_BG);

    for(S32 i=0;i<4;++i)
    {
        const std::string prefix=std::string("CineLightRig")+ROLE[i];
        mYawNames[i]=prefix+"Yaw";
        mPitchNames[i]=prefix+"Pitch";
        mOnNames[i]=prefix+"On";
        mOffsetNames[i]=prefix+"OffsetZ";
        refreshSetting(i);
        for(const std::string*name:
        {
            &mYawNames[i],&mPitchNames[i],&mOnNames[i],&mOffsetNames[i]
        }
        )if(LLControlVariable*c=gSavedSettings.getControl(*name).get())mConnections.emplace_back(c->getSignal()->connect([this,i](LLControlVariable*,const LLSD&,const LLSD&)
        {
            if(hasMouseCapture()&&!mWriting)cancelDrag();refreshSetting(i);
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl(mViewSetting).get())
    {
        mView=c->get().asInteger();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            cancelDrag();mView=llclamp(v.asInteger(),0,2);
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl(mSideSetting).get())
    {
        mSideFrom=c->get().asInteger();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            cancelDrag();mSideFrom=llclamp(v.asInteger(),0,1);
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl("CineLightRigMirror").get())
    {
        mMirror=c->get().asBoolean();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            if(hasMouseCapture())cancelDrag();mMirror=v.asBoolean();
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl("CineLightRigRadius").get())
    {
        mRigRadius=(F32)c->get().asReal();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            if(hasMouseCapture())cancelDrag();mRigRadius=(F32)v.asReal();
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl("CineLightRigOrbitYaw").get())
    {
        mOrbitYaw=(F32)c->get().asReal();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            if(hasMouseCapture())cancelDrag();mOrbitYaw=(F32)v.asReal();
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl("CineLightRigOrbitPitch").get())
    {
        mOrbitPitch=(F32)c->get().asReal();
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            if(hasMouseCapture())cancelDrag();mOrbitPitch=(F32)v.asReal();
        }
        ));
    }

    if(LLControlVariable*c=gSavedSettings.getControl("DirectorV2SelectedLight").get())
    {
        mSelected=llclamp(c->get().asInteger(),0,3);
        mConnections.emplace_back(c->getSignal()->connect([this](LLControlVariable*,const LLSD&v,const LLSD&)
        {
            select(v.asInteger());
        }
        ));
    }

}

void ALLightRigMap::setViewMode(S32 view)
{
    view=llclamp(view,0,2);
    if(gSavedSettings.controlExists(mViewSetting))gSavedSettings.setS32(mViewSetting,view);
    else mView=view;
}

bool ALLightRigMap::postBuild()
{
    if(!LLUICtrl::postBuild())return false;
    LLHandle<LLUICtrl>handle=getHandle();
    doPeriodically([handle]()
    {
        LLUICtrl*ctrl=handle.get();if(!ctrl)return true;ALLightRigMap*map=dynamic_cast<ALLightRigMap*>(ctrl);if(isWidgetOnScreen(map))map->refreshLiveReadouts();return false;
    }
    ,.1f);
    return true;
}

void ALLightRigMap::refreshSetting(S32 i)
{
    mYaw[i]=gSavedSettings.getF32(mYawNames[i]);
    mPitch[i]=gSavedSettings.getF32(mPitchNames[i]);
    mOn[i]=gSavedSettings.getBOOL(mOnNames[i]);
    mOffset[i]=gSavedSettings.getF32(mOffsetNames[i]);
    refreshLegend(i);
}

void ALLightRigMap::refreshLegend(S32 i)
{
    mLegendText[i]=ALKitMath::yawSideText(mYaw[i],mYaw[i])+" / "+llformat("%.0f\xC2\xB0",mPitch[i]);
}

void ALLightRigMap::refreshLiveReadouts()
{
    const LLVector3 at=LLViewerCamera::getInstance()->getAtAxis();
    if(at.mV[VX]*at.mV[VX]+at.mV[VY]*at.mV[VY]>=.0025f)mCameraBearing=atan2f(-at.mV[VY],-at.mV[VX])*RAD_TO_DEG;
    const auto&live=ALCineLightRigManager::instance().selected().lastFrame();
    bool selected_live=false;
    F32 distance=mRigRadius;
    for(S32 i=0;i<4;++i)
    {
        const auto&e=live.mProj[i];
        const bool usable=fabsf(e.mOffX)+fabsf(e.mOffY)+fabsf(e.mOffZ)>1e-4f;
        const F32 t=usable?ALKitMath::wrap180(atan2f(e.mOffY,e.mOffX)*RAD_TO_DEG-mCameraBearing):ALKitMath::wrap180(mYaw[i]+mOrbitYaw-mCameraBearing);
        mLegendText[i]=ALKitMath::yawSideText(t,mYaw[i])+" / "+llformat("%.0f\xC2\xB0",mPitch[i]);
        if(i==mSelected&&usable)
        {
            selected_live=true;
            distance=sqrtf(e.mOffX*e.mOffX+e.mOffY*e.mOffY+e.mOffZ*e.mOffZ);
        }
    }
    mFooterText=selected_live?llformat("Distance %.2f m",distance):llformat("Unmirrored preview \xC2\xB7 rig off \xC2\xB7 %.2f m",distance);
}

void ALLightRigMap::cancelDrag()
{
    if(hasMouseCapture())gFocusMgr.setMouseCapture(nullptr);
    mDragRole=-1;
}

S32 ALLightRigMap::heightForWidth(S32 width)const
{
    return llclamp(ll_round(.62f*width),180,240)+ALKitStyle::MAP_LEGEND_H;
}

void ALLightRigMap::select(S32 role)
{
    mSelected=llclamp(role,0,3);
}

void ALLightRigMap::draw()
{

    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("ALLightRigMap draw");

    const LLRect rect=getLocalRect();
    const F32 canvas_h=llmin((F32)(rect.getHeight()-ALKitStyle::MAP_LEGEND_H),(F32)llclamp(ll_round(.62f*rect.getWidth()),180,240));
    mCanvasCx=rect.getWidth()*.5f;
    mCanvasCy=ALKitStyle::MAP_LEGEND_H+canvas_h*.5f;
    mCanvasRadius=llmin(.42f*canvas_h,.30f*rect.getWidth());

    const LLVector3 at=LLViewerCamera::getInstance()->getAtAxis();
    if(at.mV[VX]*at.mV[VX]+at.mV[VY]*at.mV[VY]>=.0025f)mCameraBearing=atan2f(-at.mV[VY],-at.mV[VX])*RAD_TO_DEG;

    gl_rect_2d(0,ll_round(ALKitStyle::MAP_LEGEND_H+canvas_h),rect.getWidth(),ALKitStyle::MAP_LEGEND_H,ALKitStyle::color(ALKitStyle::DIAL_FACE).get(),true);

    const LLColor4 track=ALKitStyle::color(ALKitStyle::ARC_TRACK).get(),dim=ALKitStyle::color(ALKitStyle::DIM).get(),accent=ALKitStyle::color(ALKitStyle::ACCENT).get();

    if(mView==ALKitMath::SIDE)gl_line_2d(ll_round(mCanvasCx-mCanvasRadius),ll_round(mCanvasCy),ll_round(mCanvasCx+mCanvasRadius),ll_round(mCanvasCy),track);
    else if(mView==ALKitMath::TOP)
    {
        gGL.color4fv(track.mV);
        gl_circle_2d(mCanvasCx,mCanvasCy,mCanvasRadius,48,false);
    }
    else ellipse(mCanvasCx,mCanvasCy,mCanvasRadius,mCanvasRadius*sinf(24.f*DEG_TO_RAD),track);

    if(mView!=ALKitMath::SIDE)gl_line_2d(ll_round(mCanvasCx),ll_round(mCanvasCy-mCanvasRadius),ll_round(mCanvasCx),ll_round(mCanvasCy+mCanvasRadius),track);

    const auto&rig=ALCineLightRigManager::instance().selected();
    const auto&live=rig.lastFrame();

    struct DrawLight
    {
        S32 i;
        F32 x,y,depth,t,p,front,under,rr;
        bool live;
        LLColor4 color;
    }
    ;
    DrawLight lights[4];

    std::array<LLColor4,4>role_colors;
    for(S32 i=0;i<4;++i)
    {
        const auto&e=live.mProj[i];
        const bool usable=fabsf(e.mOffX)+fabsf(e.mOffY)+fabsf(e.mOffZ)>1e-4f;
        F32 t,p;
        if(i==mDragRole)
        {
            t=mDragDisplayYaw;
            p=mDragDisplayPitch;
        }
        else if(usable)
        {
            t=ALKitMath::wrap180(atan2f(e.mOffY,e.mOffX)*RAD_TO_DEG-mCameraBearing);
            p=atan2f(e.mOffZ,hypotf(e.mOffX,e.mOffY))*RAD_TO_DEG;
        }
        else
        {
            t=ALKitMath::wrap180(mYaw[i]+mOrbitYaw-mCameraBearing);
            p=ALKitMath::pDisp(mPitch[i],mOrbitPitch,mOffset[i],mRigRadius);
        }

        ALKitMath::projectLight(mView,mSideFrom,t,p,mCanvasRadius,mCanvasCx,mCanvasCy,&lights[i].x,&lights[i].y,&lights[i].depth);
        lights[i].i=i;
        lights[i].t=t;
        lights[i].p=p;
        lights[i].front=ALKitMath::frontFactor(mView,mSideFrom,t);
        lights[i].under=p<0.f?llmin(1.f,-p/40.f):0.f;
        lights[i].live=usable;
        LLColor4 c=ALKitStyle::color(i==0?ALKitStyle::MAP_KEY:i==1?ALKitStyle::MAP_FILL:i==2?ALKitStyle::MAP_BACK:ALKitStyle::MAP_BG).get();
        if(usable&&mOn[i])
        {
            const F32 mx=llmax(e.mSR,llmax(e.mSG,e.mSB));
            if(mx>1e-4f)c.set(e.mSR/mx,e.mSG/mx,e.mSB/mx,1.f);
        }
        if(!mOn[i])c=ALKitStyle::color(ALKitStyle::OFF_GREY).get();
        lights[i].color=c;
        role_colors[i]=c;
        const F32 dn=llclamp(lights[i].depth/mCanvasRadius,-1.f,1.f);
        lights[i].rr=mOn[i]?4.6f+2.6f*lights[i].front+.8f*dn:6.2f+.6f*dn;
        mScreenX[i]=lights[i].x;
        mScreenY[i]=lights[i].y;
        mHitRadius[i]=lights[i].rr;
    }

    std::sort(std::begin(lights),std::end(lights),[](const DrawLight&a,const DrawLight&b)
    {
        return a.depth<b.depth;
    }
    );
    for(S32 i=0;i<4;++i)mDrawOrder[i]=lights[i].i;

    const F32 chest_y=mCanvasCy+(mView==ALKitMath::TOP?0.f:.33f*mCanvasRadius);

    auto paint=[&](const DrawLight&l)
    {
        const S32 i=l.i;
        LLColor4 c=l.color;
        const F32 rr=l.rr;
        if(!mOn[i])
        {
            gGL.color4fv(ALKitStyle::color(ALKitStyle::CONTROL_BG).get().mV);
            gl_circle_2d(l.x,l.y,rr,20,true);
            gGL.color4fv(c.mV);
            gl_circle_2d(l.x,l.y,rr,20,false);
            gl_line_2d(ll_round(l.x-rr*.51f),ll_round(l.y-rr*.51f),ll_round(l.x+rr*.51f),ll_round(l.y+rr*.51f),c);
            label(S_OFF,l.x,l.y-rr-7.f,c);
        }

        else if(l.front>=.5f)
        {
            LLColor4 glow=c%(0.26f*l.front*l.front*(1.f-.5f*l.under));
            gGL.color4fv(glow.mV);
            gl_circle_2d(l.x,l.y,rr+6.f,20,true);
            LLColor4 fill=c%((.5f+.5f*l.front)*(1.f-.35f*l.under));
            gGL.color4fv(fill.mV);
            gl_circle_2d(l.x,l.y,rr,20,true);
            gl_line_2d(ll_round(l.x),ll_round(l.y),ll_round(mCanvasCx),ll_round(chest_y),c%((.2f+.45f*l.front)*(1.f-.5f*l.under)));
            if(l.under>.05f)dashedCircle(l.x,l.y,rr,c);
        }

        else
        {
            gGL.color4fv((c%(.35f+.65f*l.front)).mV);
            gl_circle_2d(l.x,l.y,rr,20,true);
            gGL.color4fv(c.mV);
            if(l.under>.05f)dashedCircle(l.x,l.y,rr,c);
            else gl_circle_2d(l.x,l.y,rr,20,false);
            dashed(l.x,l.y,mCanvasCx,chest_y,3.f,3.f,c%(.4f));
        }

        if(i==mSelected)
        {
            gGL.color4fv((c%.6f).mV);
            gl_circle_2d(l.x,l.y,rr+4.f,24,false);
        }
        label(LABEL[i],l.x+(l.x>=mCanvasCx?rr+4.f:-rr-4.f),l.y,c,l.x>=mCanvasCx?LLFontGL::LEFT:LLFontGL::RIGHT);
    }
    ;

    for(const DrawLight&l:lights)if(mView!=ALKitMath::TOP&&mOn[l.i]&&l.front<.5f)paint(l);

    const LLColor4 body=ALKitStyle::color(ALKitStyle::DIAL_RING).get();
    gGL.color4fv(body.mV);

    if(mView==ALKitMath::TOP)
    {
        gl_circle_2d(mCanvasCx,mCanvasCy,6.f,20,true);
        gl_triangle_2d(ll_round(mCanvasCx-4),ll_round(mCanvasCy-4),ll_round(mCanvasCx+4),ll_round(mCanvasCy-4),ll_round(mCanvasCx),ll_round(mCanvasCy-10),body,true);
    }

    else if(mView==ALKitMath::SIDE)
    {
        const F32 s=mSideFrom==0?1.f:-1.f;
        gl_circle_2d(mCanvasCx,mCanvasCy+22.f,12.f,20,true);
        LLColor4 hair=body%.7f;
        gGL.color4fv(hair.mV);
        gl_circle_2d(mCanvasCx-s*6.f,mCanvasCy+24.f,7.f,16,true);
        gGL.color4fv(body.mV);
        gl_rect_2d(ll_round(mCanvasCx-9),ll_round(mCanvasCy+12),ll_round(mCanvasCx+9),ll_round(mCanvasCy-28),body,true);
        gl_triangle_2d(ll_round(mCanvasCx+s*8),ll_round(mCanvasCy+26),ll_round(mCanvasCx+s*17),ll_round(mCanvasCy+22),ll_round(mCanvasCx+s*8),ll_round(mCanvasCy+19),body,true);
        LLColor4 near_arm=ALKitStyle::color(ALKitStyle::DIM).get();
        gl_line_2d(ll_round(mCanvasCx+s*8),ll_round(mCanvasCy+6),ll_round(mCanvasCx+s*13),ll_round(mCanvasCy-16),near_arm);
        gl_triangle_2d(ll_round(mCanvasCx-7),ll_round(mCanvasCy-28),ll_round(mCanvasCx+s*13),ll_round(mCanvasCy-28),ll_round(mCanvasCx-7),ll_round(mCanvasCy-33),body,true);
        gl_triangle_2d(ll_round(mCanvasCx+7),ll_round(mCanvasCy-28),ll_round(mCanvasCx+s*17),ll_round(mCanvasCy-28),ll_round(mCanvasCx+7),ll_round(mCanvasCy-33),body,true);
        gGL.color4fv(dim.mV);
        gl_circle_2d(mCanvasCx+s*6.f,mCanvasCy+24.f,2.f,10,true);
        gGL.color4fv(body.mV);
        gl_circle_2d(mCanvasCx+s*6.f,mCanvasCy+24.f,.8f,8,true);
        dashed(mCanvasCx+s*15.f,mCanvasCy+23.f,mCanvasCx+s*45.f,mCanvasCy+23.f,3.f,3.f,accent);
    }

    else
    {
        gl_circle_2d(mCanvasCx,mCanvasCy+18.f,10.f,20,true);
        gl_rect_2d(ll_round(mCanvasCx-8),ll_round(mCanvasCy+9),ll_round(mCanvasCx+8),ll_round(mCanvasCy-24),body,true);
        gGL.color4fv(dim.mV);
        gl_circle_2d(mCanvasCx-3.f,mCanvasCy+20.f,1.5f,8,true);
        gl_circle_2d(mCanvasCx+3.f,mCanvasCy+20.f,1.5f,8,true);
    }

    if(mView==ALKitMath::PERSPECTIVE)nearEllipse(mCanvasCx,mCanvasCy,mCanvasRadius,mCanvasRadius*sinf(24.f*DEG_TO_RAD),track);
    for(const DrawLight&l:lights)if(mView==ALKitMath::TOP||!mOn[l.i]||l.front>=.5f)paint(l);

    if(mView==ALKitMath::SIDE)
    {
        const F32 cam_x=mSideFrom==0?mCanvasCx+mCanvasRadius:mCanvasCx-mCanvasRadius;
        label(S_CAM,cam_x,mCanvasCy-12.f,dim);
        label(S_BACK,2.f*mCanvasCx-cam_x,mCanvasCy-12.f,dim);
        const F32 chip_y=mCanvasCy+mCanvasRadius-21.f;
        gl_rect_2d(8,ll_round(chip_y+13),126,ll_round(chip_y-13),ALKitStyle::color(ALKitStyle::CONTROL_BG).get(),true);
        gGL.color4fv(track.mV);
        gl_circle_2d(22.f,chip_y,8.f,18,false);
        gGL.color4fv(body.mV);
        gl_circle_2d(22.f,chip_y,2.f,10,true);
        gl_triangle_2d(20,ll_round(chip_y-7),24,ll_round(chip_y-7),22,ll_round(chip_y-11),accent,true);
        const F32 eye_x=mSideFrom==0?15.f:29.f;
        gl_circle_2d(eye_x,chip_y,1.5f,8,false);
        dashed(eye_x,chip_y,22.f,chip_y,1.5f,1.5f,dim);
        label(mSideFrom==0?S_SIDE_L:S_SIDE_R,36.f,chip_y,ALKitStyle::color(ALKitStyle::TEXT).get(),LLFontGL::LEFT);
    }

    else
    {
        label(S_CAM_L,mCanvasCx-mCanvasRadius,mCanvasCy,dim);
        label(S_CAM_R,mCanvasCx+mCanvasRadius,mCanvasCy,dim);
        label(S_BACK,mCanvasCx,mCanvasCy+mCanvasRadius+8.f,dim);
        label(S_CAM,mCanvasCx,mCanvasCy-mCanvasRadius-8.f,dim);
    }

    label(mView==ALKitMath::SIDE?S_SIDE_KEY:S_OTHER_KEY,mCanvasCx,ALKitStyle::MAP_LEGEND_H+7.f,dim);
    label(mFooterText,rect.getWidth()-8.f,ALKitStyle::MAP_LEGEND_H+7.f,dim,LLFontGL::RIGHT);

    const F32 chip_w=rect.getWidth()/4.f;
    for(S32 i=0;i<4;++i)
    {
        const F32 left=i*chip_w;
        const LLColor4 c=role_colors[i];
        if(i==mSelected)gl_rect_2d(ll_round(left+2),ALKitStyle::MAP_LEGEND_H-2,ll_round(left+chip_w-2),2,c,false);
        gGL.color4fv(c.mV);
        gl_circle_2d(left+10.f,20.f,4.f,12,mOn[i]);
        label(LABEL[i],left+18.f,27.f,ALKitStyle::color(ALKitStyle::TEXT).get(),LLFontGL::LEFT);
        label(mLegendText[i],left+18.f,13.f,dim,LLFontGL::LEFT);
    }

    LLUICtrl::draw();

}

void ALLightRigMap::beginDrag(S32 role,S32 x,S32 y)
{

    if(ALKitShell*shell=ALKitShell::find(this))
    {
        std::string page;
        for(LLView*v=getParent();v;v=v->getParent())if(ALKitPage*p=dynamic_cast<ALKitPage*>(v))
        {
            page=p->pageKey();
            break;
        }
        shell->history().beginGesture(ALKitHistory::MOUSE,page);
    }

    select(role);
    if(gSavedSettings.controlExists("DirectorV2SelectedLight"))gSavedSettings.setS32("DirectorV2SelectedLight",role);
    const auto&rig=ALCineLightRigManager::instance().selected();
    const auto&e=rig.mapTargetFrame().mProj[role];
    const auto&shown=rig.lastFrame().mProj[role];
    mLiveTarget=fabsf(shown.mOffX)+fabsf(shown.mOffY)+fabsf(shown.mOffZ)>1e-4f;
    mDrag={};
    mDrag.x0=(F32)x;
    mDrag.y0=(F32)y;
    mDrag.cx=mCanvasCx;
    mDrag.cy=mCanvasCy;
    mDrag.base_yaw0=mYaw[role];
    mDrag.base_pitch0=mPitch[role];
    mDrag.orbit_yaw0=mOrbitYaw;
    mDrag.orbit_pitch0=mOrbitPitch;
    mDrag.offset_z0=mOffset[role];
    mDrag.radius=mLiveTarget&&e.mLightRadius>0.f?e.mLightRadius/2.2f:mRigRadius;
    mDrag.canvas_radius=mCanvasRadius;
    mDrag.t0=mLiveTarget?ALKitMath::wrap180(atan2f(e.mOffY,e.mOffX)*RAD_TO_DEG-mCameraBearing):ALKitMath::wrap180(mYaw[role]+mOrbitYaw-mCameraBearing);
    mDrag.p0=ALKitMath::pDisp(mPitch[role],mDrag.orbit_pitch0,mDrag.offset_z0,mDrag.radius);
    mDrag.view=mView;
    mDrag.side_from=mSideFrom;
    mDrag.mirror=mMirror;
    mDrag.live=mLiveTarget;
    mDrag.camera_bearing0=mCameraBearing;
    mDrag.sigma=(mLiveTarget&&mMirror)?-1.f:1.f;
    F32 px,py;
    ALKitMath::projectLight(mDrag.view,mDrag.side_from,mDrag.t0,mDrag.p0,mDrag.canvas_radius,mDrag.cx,mDrag.cy,&px,&py,&mDrag.depth0);
    mDrag.grab_x=(F32)x-px;
    mDrag.grab_y=(F32)y-py;
    mDragDisplayYaw=mDrag.t0;
    mDragDisplayPitch=mDrag.p0;
    mDragRole=role;
    mDragWrote=false;
    gFocusMgr.setMouseCapture(this);
    setFocus(true);

}

bool ALLightRigMap::handleMouseDown(S32 x,S32 y,MASK)
{
    if(!getEnabled())return false;
    if(y<ALKitStyle::MAP_LEGEND_H)
    {
        select(llclamp((4*x)/llmax(1,getRect().getWidth()),0,3));
        if(gSavedSettings.controlExists("DirectorV2SelectedLight"))gSavedSettings.setS32("DirectorV2SelectedLight",mSelected);
        return true;
    }
    for(S32 n=3;n>=0;--n)
    {
        const S32 i=mDrawOrder[n];
        const F32 dx=x-mScreenX[i],dy=y-mScreenY[i];
        if(dx*dx+dy*dy<=(mHitRadius[i]+4.f)*(mHitRadius[i]+4.f))
        {
            beginDrag(i,x,y);
            return true;
        }
    }
    if(mView==ALKitMath::SIDE&&x>=8&&x<=134&&y>=mCanvasCy+mCanvasRadius-34&&y<=mCanvasCy+mCanvasRadius-8)
    {
        mSideFrom=1-mSideFrom;
        if(gSavedSettings.controlExists(mSideSetting))gSavedSettings.setS32(mSideSetting,mSideFrom);
        return true;
    }
    return false;
}

void ALLightRigMap::applyDrag(S32 x,S32 y)
{
    if(mDragRole<0||(!mDragWrote&&x==mDrag.x0&&y==mDrag.y0))return;
    ALKitMath::DragResult result;
    if(mDrag.view==ALKitMath::TOP)result=ALKitMath::dragTopAbsolute(mDrag,(F32)x,(F32)y);
    else if(mDrag.view==ALKitMath::SIDE)result=ALKitMath::dragSideAbsolute(mDrag,(F32)x,(F32)y);
    else result=ALKitMath::drag25DAbsolute(mDrag,(F32)x,(F32)y);
    mWriting=true;
    gSavedSettings.setF32(mYawNames[mDragRole],result.yaw);
    gSavedSettings.setF32(mPitchNames[mDragRole],result.pitch);
    mWriting=false;
    mDragWrote=true;
    mDragDisplayYaw=ALKitMath::wrap180(mDrag.t0+mDrag.sigma*ALKitMath::wrap180(result.yaw-mDrag.base_yaw0));
    mDragDisplayPitch=ALKitMath::pDisp(result.pitch,mDrag.orbit_pitch0,mDrag.offset_z0,mDrag.radius);
}

bool ALLightRigMap::handleHover(S32 x,S32 y,MASK)
{
    if(hasMouseCapture())
    {
        applyDrag(x,y);
        return true;
    }
    const bool side_chip=mView==ALKitMath::SIDE&&x>=8&&x<=134&&y>=mCanvasCy+mCanvasRadius-34&&y<=mCanvasCy+mCanvasRadius-8;
    const char*tip="";
    if(side_chip)tip=mSideFrom==0
        ? "Side view from camera-left (the subject's right side). Solid = lights on this side, hollow = far side behind the avatar. Click to look from the other side."
        : "Side view from camera-right (the subject's left side). Solid = lights on this side, hollow = far side behind the avatar. Click to look from the other side.";
    setToolTipIfChanged(this,tip);
    return LLUICtrl::handleHover(x,y,0);
}

bool ALLightRigMap::handleMouseUp(S32 x,S32 y,MASK)
{
    if(!hasMouseCapture())return false;
    applyDrag(x,y);
    gFocusMgr.setMouseCapture(nullptr);
    mDragRole=-1;
    onCommit();
    return true;
}

bool ALLightRigMap::handleKeyHere(KEY key,MASK mask)
{
    const F32 step=(mask&MASK_SHIFT)?10.f:1.f;
    if(key==KEY_LEFT||key==KEY_RIGHT)
    {
        gSavedSettings.setF32(mYawNames[mSelected],ALKitMath::wrap180(mYaw[mSelected]+(key==KEY_RIGHT?step:-step)));
        return true;
    }
    if(key==KEY_UP||key==KEY_DOWN)
    {
        gSavedSettings.setF32(mPitchNames[mSelected],llclamp(mPitch[mSelected]+(key==KEY_UP?step:-step),-85.f,85.f));
        return true;
    }
    if(key=='['||key==']')
    {
        select((mSelected+(key==']'?1:3))%4);
        if(gSavedSettings.controlExists("DirectorV2SelectedLight"))gSavedSettings.setS32("DirectorV2SelectedLight",mSelected);
        return true;
    }
    if(key==KEY_HOME)
    {
        if(LLControlVariable*c=gSavedSettings.getControl(mYawNames[mSelected]).get())c->resetToDefault(true);
        if(LLControlVariable*c=gSavedSettings.getControl(mPitchNames[mSelected]).get())c->resetToDefault(true);
        return true;
    }
    return LLUICtrl::handleKeyHere(key,mask);
}

bool ALLightRigMap::handleUnicodeCharHere(llwchar c)
{
    if(c=='['||c==']')
    {
        select((mSelected+(c==']'?1:3))%4);
        if(gSavedSettings.controlExists("DirectorV2SelectedLight"))gSavedSettings.setS32("DirectorV2SelectedLight",mSelected);
        return true;
    }
    return LLUICtrl::handleUnicodeCharHere(c);
}
