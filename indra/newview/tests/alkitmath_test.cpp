#include "linden_common.h"
#include "../test/lltut.h"
#include "../alkitmath.h"

namespace tut
{
struct alkitmath_data {};
typedef test_group<alkitmath_data> alkitmath_group;
typedef alkitmath_group::object alkitmath_object;
alkitmath_group gAlKitMath("alkitmath");

template<> template<> void alkitmath_object::test<1>()
{
    ensure_approximately_equals_range("wrap +", ALKitMath::wrap180(181.f), -179.f, .001f);
    ensure_approximately_equals_range("wrap -", ALKitMath::wrap180(-181.f), 179.f, .001f);
    ensure_approximately_equals_range("wrap endpoint", ALKitMath::wrap180(180.f), -180.f, .001f);
}

template<> template<> void alkitmath_object::test<2>()
{
    for (const auto range : { std::pair<float,float>{-85.f,85.f}, {-180.f,180.f}, {0.f,360.f} })
    {
        for (float value : {range.first, (range.first + range.second) * .5f, range.second})
        {
            ensure_approximately_equals_range("arc round trip", ALKitMath::arcValue(ALKitMath::arcAngle(value, range.first, range.second), range.first, range.second), value, .001f);
        }
    }
    ensure_approximately_equals_range("horizon right", ALKitMath::arcAngle(0.f, -90.f, 90.f), 0.f, .001f);
}

template<> template<> void alkitmath_object::test<3>()
{
    float x, y, d;
    ALKitMath::projectLight(ALKitMath::PERSPECTIVE, 0, 0.f, 0.f, 100.f, 100.f, 100.f, &x, &y, &d);
    ensure("camera below centre", y < 100.f);
    ALKitMath::projectLight(ALKitMath::SIDE, 0, -45.f, 0.f, 100.f, 100.f, 100.f, &x, &y, &d);
    const float left_depth = d;
    ALKitMath::projectLight(ALKitMath::SIDE, 0, 45.f, 0.f, 100.f, 100.f, 100.f, &x, &y, &d);
    ensure("side layers differ", left_depth != d);
    ensure("side factor differs", ALKitMath::frontFactor(ALKitMath::SIDE, 0, -45.f) != ALKitMath::frontFactor(ALKitMath::SIDE, 0, 45.f));
    ensure_equals("yaw R", ALKitMath::yawSideText(45.f, 45.f), std::string("R45\xC2\xB0"));
    ensure_equals("yaw L", ALKitMath::yawSideText(-135.f, -135.f), std::string("L135\xC2\xB0"));
}

template<> template<> void alkitmath_object::test<4>()
{
    for (float radius : {3.f, 5.25f})
    {
        const float displayed = ALKitMath::pDisp(20.f, 7.f, .4f, radius);
        ensure_approximately_equals_range("pitch inverse", ALKitMath::baseFromDisplayPitch(displayed, 7.f, .4f, radius, 20.f), 20.f, .01f);
    }
}

template<> template<> void alkitmath_object::test<5>()
{
    ALKitMath::DragStart start;
    start.x0 = start.cx = 100.f; start.y0 = 0.f; start.cy = 100.f;
    start.radius = 100.f; start.t0 = start.base_yaw0 = 25.f;
    start.p0 = ALKitMath::pDisp(10.f, 5.f, .2f, start.radius);
    start.base_pitch0 = 10.f; start.orbit_pitch0 = 5.f; start.offset_z0 = .2f;
    const auto same = ALKitMath::dragTopAbsolute(start, start.x0, start.y0);
    ensure_approximately_equals_range("top no movement yaw", same.yaw, start.base_yaw0, .001f);
    ensure_approximately_equals_range("top no movement pitch", same.pitch, start.base_pitch0, .01f);
    start.sigma = -1.f;
    const auto mirrored = ALKitMath::dragTopAbsolute(start, 200.f, 100.f);
    ensure("mirror reverses", mirrored.yaw < start.base_yaw0);
}

template<> template<> void alkitmath_object::test<6>()
{
    ensure_approximately_equals_range("unmirrored", ALKitMath::effYaw(20.f, 5.f, false, 40.f), 25.f, .001f);
    ensure_approximately_equals_range("mirrored", ALKitMath::effYaw(20.f, 5.f, true, 40.f), 55.f, .001f);
    float last = 0.f;
    for (int i = -90; i <= 90; ++i)
    {
        const float current = ALKitMath::frontFactor(ALKitMath::PERSPECTIVE, 0, static_cast<float>(i));
        ensure("front monotone toward zero", current + .001f >= last || i > 0);
        if (i <= 0) last = current;
    }
}

template<> template<> void alkitmath_object::test<7>()
{
    auto wide = ALKitMath::computeShellLayout(1160, 720);
    ensure_equals("wide rail", wide.rail, 176);
    ensure("wide help", wide.help_panel);
    ensure("wide track", wide.track >= 120);
    auto strip = ALKitMath::computeShellLayout(1141, 720, wide);
    ensure("narrowed help strip", !strip.help_panel);
    auto narrow = ALKitMath::computeShellLayout(850, 560);
    ensure_equals("narrow rail", narrow.rail, 56);
    ensure("narrow track", narrow.track >= 120);
}

template<> template<> void alkitmath_object::test<8>()
{
    const auto packed = ALKitMath::packRows({100, -1, 100, 0, 100}, {20, 99, 30, 12, 10}, 2);
    ensure("hidden", packed[1].hidden);
    ensure("spanning", packed[3].spanning);
    ensure("span begins after row", packed[3].y >= 30);
}

template<> template<> void alkitmath_object::test<9>()
{
    float x0,y0,d0,x1,y1,d1;
    ALKitMath::projectLight(ALKitMath::SIDE,0,-45.f,10.f,80.f,100.f,100.f,&x0,&y0,&d0);
    ALKitMath::projectLight(ALKitMath::SIDE,1,-45.f,10.f,80.f,100.f,100.f,&x1,&y1,&d1);
    ensure_approximately_equals_range("side directions mirror x",x0+x1,200.f,.001f);
    ensure_approximately_equals_range("side directions keep y",y0,y1,.001f);
    ensure("camera-left near",d0>0.f); ensure("camera-right far",d1<0.f);
    ensure_equals("yaw axis zero",ALKitMath::yawSideText(0.f,0.f),std::string("0\xC2\xB0"));
    ensure_equals("yaw axis back",ALKitMath::yawSideText(180.f,180.f),std::string("180\xC2\xB0"));
}

template<> template<> void alkitmath_object::test<10>()
{
    const float canvas_radius = 120.f;
    const float world_radius = 2.3f;
    for (int view : {ALKitMath::TOP, ALKitMath::PERSPECTIVE, ALKitMath::SIDE})
    for (int side : {0, 1})
    for (float sigma : {1.f, -1.f})
    for (float target_yaw : {32.f, 148.f})
    {
        ALKitMath::DragStart s;
        s.view = view;
        s.side_from = side;
        s.sigma = sigma;
        s.radius = world_radius;
        s.canvas_radius = canvas_radius;
        s.cx = 150.f;
        s.cy = 140.f;
        s.base_yaw0 = s.t0 = target_yaw > 90.f ? 126.f : -41.f;
        s.base_pitch0 = 9.f;
        s.orbit_pitch0 = 7.f;
        s.offset_z0 = .4f;
        s.p0 = ALKitMath::pDisp(s.base_pitch0, s.orbit_pitch0, s.offset_z0, world_radius);
        ALKitMath::projectLight(view, side, s.t0, s.p0, canvas_radius,
                                s.cx, s.cy, &s.x0, &s.y0, &s.depth0);
        const float target_base_pitch = view == ALKitMath::TOP ? s.base_pitch0 : 21.f;
        const float target_pitch = ALKitMath::pDisp(target_base_pitch, s.orbit_pitch0,
                                                     s.offset_z0, world_radius);
        float x, y, depth;
        ALKitMath::projectLight(view, side, target_yaw, target_pitch, canvas_radius,
                                s.cx, s.cy, &x, &y, &depth);
        ensure("pointer moved", x != s.x0 || y != s.y0);
        const auto result = view == ALKitMath::TOP
            ? ALKitMath::dragTopAbsolute(s, x, y)
            : view == ALKitMath::SIDE ? ALKitMath::dragSideAbsolute(s, x, y)
                                      : ALKitMath::drag25DAbsolute(s, x, y);
        ensure_approximately_equals_range("moved inverse yaw", result.yaw,
            ALKitMath::wrap180(s.base_yaw0 + sigma * ALKitMath::wrap180(target_yaw - s.t0)), .02f);
        ensure_approximately_equals_range("moved inverse pitch", result.pitch, target_base_pitch, .02f);
    }
}

template<> template<> void alkitmath_object::test<11>()
{
    for(int view : {ALKitMath::PERSPECTIVE,ALKitMath::SIDE}) for(int side : {0,1})
    {
        ALKitMath::DragStart s;s.view=view;s.side_from=side;s.radius=100.f;s.canvas_radius=100.f;s.cx=120.f;s.cy=100.f;s.base_yaw0=s.t0=-54.f;s.base_pitch0=18.f;s.orbit_pitch0=9.f;s.offset_z0=.35f;s.p0=ALKitMath::pDisp(18.f,9.f,.35f,100.f);
        ALKitMath::projectLight(view,side,s.t0,s.p0,s.radius,s.cx,s.cy,&s.x0,&s.y0,&s.depth0);
        ALKitMath::DragResult first;
        for(int i=0;i<1000;++i)first=view==ALKitMath::SIDE?ALKitMath::dragSideAbsolute(s,s.x0+18.f,s.y0+9.f):ALKitMath::drag25DAbsolute(s,s.x0+18.f,s.y0+9.f);
        const auto once=view==ALKitMath::SIDE?ALKitMath::dragSideAbsolute(s,s.x0+18.f,s.y0+9.f):ALKitMath::drag25DAbsolute(s,s.x0+18.f,s.y0+9.f);
        ensure("out path changes yaw or pitch",ALKitMath::angularDistance(once.yaw,s.base_yaw0)>.01f||std::fabs(once.pitch-s.base_pitch0)>.01f);
        ensure_approximately_equals_range("fixed pointer yaw no drift",first.yaw,once.yaw,.0001f);ensure_approximately_equals_range("fixed pointer pitch no drift",first.pitch,once.pitch,.0001f);
        const auto back=view==ALKitMath::SIDE?ALKitMath::dragSideAbsolute(s,s.x0,s.y0):ALKitMath::drag25DAbsolute(s,s.x0,s.y0);
        ensure_approximately_equals_range("out back yaw",back.yaw,s.base_yaw0,.01f);ensure_approximately_equals_range("out back pitch",back.pitch,s.base_pitch0,.01f);
    }
}

template<> template<> void alkitmath_object::test<12>()
{
    ALKitMath::DragStart s;s.radius=100.f;s.canvas_radius=100.f;s.cx=100.f;s.cy=100.f;s.x0=100.f;s.y0=0.f;s.t0=s.base_yaw0=179.f;s.base_pitch0=s.p0=0.f;
    const auto positive=ALKitMath::dragTopAbsolute(s,200.f,100.f);s.sigma=-1.f;const auto mirrored=ALKitMath::dragTopAbsolute(s,200.f,100.f);
    ensure("mirror sign",ALKitMath::wrap180(positive.yaw-s.base_yaw0)==-ALKitMath::wrap180(mirrored.yaw-s.base_yaw0));
    const auto home=ALKitMath::dragTopAbsolute(s,s.x0,s.y0);ensure_approximately_equals_range("full turn periodic home",home.yaw,179.f,.01f);
}

template<> template<> void alkitmath_object::test<13>()
{
    const auto w1160=ALKitMath::computeShellLayout(1160,720);ensure_equals("1160 main",w1160.main,760);ensure_equals("1160 columns",w1160.columns,2);
    const auto w1141=ALKitMath::computeShellLayout(1141,720,w1160);ensure("1141 strip",!w1141.help_panel);ensure_equals("1141 main",w1141.main,965);
    const auto w915=ALKitMath::computeShellLayout(915,560);ensure_equals("915 icon rail",w915.rail,56);ensure_equals("915 columns",w915.columns,2);
    const auto w850=ALKitMath::computeShellLayout(850,560);ensure_equals("850 main",w850.main,794);ensure("850 track",w850.track>=120);
    ensure("1145 remains strip",!ALKitMath::computeShellLayout(1145,720,w850).help_panel);ensure("1150 opens panel",ALKitMath::computeShellLayout(1150,720,w850).help_panel);
}

template<> template<> void alkitmath_object::test<14>()
{
    for(float yaw=-180.f;yaw<=180.f;yaw+=5.f){const float a=ALKitMath::frontFactor(ALKitMath::PERSPECTIVE,0,yaw);ensure("front bounded",a>=0.f&&a<=1.f);}
    ensure("side opposite layers",ALKitMath::frontFactor(ALKitMath::SIDE,0,45.f)<.5f&&ALKitMath::frontFactor(ALKitMath::SIDE,0,-45.f)>.5f);
    ensure_approximately_equals_range("eff yaw wrap",ALKitMath::effYaw(179.f,5.f,false,0.f),-176.f,.001f);
}

template<> template<> void alkitmath_object::test<15>()
{
    ensure_approximately_equals_range("signed compass wraps",ALKitMath::wrapRange(181.f,-180.f,180.f),-179.f,.001f);
    ensure_approximately_equals_range("unsigned compass wraps",ALKitMath::wrapRange(360.f,0.f,360.f),0.f,.001f);
    const auto legacy=ALKitMath::dialGeometry(48.f,48.f,false);ensure_approximately_equals_range("legacy centre x",legacy.cx,24.f,.001f);ensure_approximately_equals_range("legacy radius",legacy.radius,12.f,.001f);ensure_approximately_equals_range("legacy letters",legacy.letter_radius,18.f,.001f);
    const auto kit=ALKitMath::dialGeometry(48.f,48.f,true);ensure_approximately_equals_range("kit control-column centre",kit.cx,24.f,.001f);ensure_approximately_equals_range("kit radius",kit.radius,20.f,.001f);
}

template<> template<> void alkitmath_object::test<16>()
{
    for(int view:{ALKitMath::TOP,ALKitMath::PERSPECTIVE,ALKitMath::SIDE})for(int side:{0,1})for(float radius:{80.f,125.f})for(float yaw:{35.f,145.f})
    {
        ALKitMath::DragStart s;s.view=view;s.side_from=side;s.radius=radius;s.canvas_radius=radius;s.cx=160.f;s.cy=130.f;s.base_yaw0=s.t0=yaw;s.base_pitch0=12.f;s.orbit_pitch0=8.f;s.offset_z0=.6f;s.p0=ALKitMath::pDisp(s.base_pitch0,s.orbit_pitch0,s.offset_z0,radius);
        ALKitMath::projectLight(view,side,s.t0,s.p0,radius,s.cx,s.cy,&s.x0,&s.y0,&s.depth0);
        ALKitMath::DragResult sample;
        for(int i=0;i<1000;++i)
        {
            const float phase=ALKitMath::PI*2.f*i/999.f;
            const float x=s.x0+20.f*std::sin(phase),y=s.y0+12.f*std::sin(phase*2.f);
            sample=view==ALKitMath::TOP?ALKitMath::dragTopAbsolute(s,x,y):view==ALKitMath::SIDE?ALKitMath::dragSideAbsolute(s,x,y):ALKitMath::drag25DAbsolute(s,x,y);
            ensure("path result finite",std::isfinite(sample.yaw)&&std::isfinite(sample.pitch));
            if(i==250)ensure("path intermediate moved",ALKitMath::angularDistance(sample.yaw,s.base_yaw0)>.01f||std::fabs(sample.pitch-s.base_pitch0)>.01f);
        }
        const auto back=view==ALKitMath::TOP?ALKitMath::dragTopAbsolute(s,s.x0,s.y0):view==ALKitMath::SIDE?ALKitMath::dragSideAbsolute(s,s.x0,s.y0):ALKitMath::drag25DAbsolute(s,s.x0,s.y0);
        ensure_approximately_equals_range("1000 out/back yaw",back.yaw,s.base_yaw0,.01f);ensure_approximately_equals_range("1000 out/back pitch",back.pitch,s.base_pitch0,.01f);
    }
    ALKitMath::DragStart pole;pole.side_from=0;pole.radius=100.f;pole.canvas_radius=100.f;pole.cx=pole.cy=100.f;pole.x0=100.f;pole.y0=0.f;pole.t0=pole.base_yaw0=-60.f;pole.base_pitch0=0.f;
    const auto at_pole=ALKitMath::dragSideAbsolute(pole,100.f,200.f);ensure_approximately_equals_range("side pole keeps side",at_pole.yaw,-60.f,.001f);
    ALKitMath::DragStart silhouette;silhouette.radius=100.f;silhouette.canvas_radius=100.f;silhouette.cx=silhouette.cy=100.f;silhouette.t0=silhouette.base_yaw0=90.f;silhouette.p0=silhouette.base_pitch0=0.f;ALKitMath::projectLight(ALKitMath::PERSPECTIVE,0,90.f,0.f,100.f,100.f,100.f,&silhouette.x0,&silhouette.y0,&silhouette.depth0);float t,p;ALKitMath::unproject25D(silhouette,silhouette.x0,silhouette.y0,&t,&p);ensure_approximately_equals_range("silhouette yaw branch",t,90.f,.01f);
}

template<> template<> void alkitmath_object::test<17>()
{
    ALKitMath::DragStart s;s.radius=100.f;s.canvas_radius=100.f;s.cx=s.cy=100.f;s.x0=100.f;s.y0=0.f;s.t0=s.base_yaw0=20.f;s.p0=s.base_pitch0=5.f;s.live=true;s.mirror=true;s.sigma=-1.f;
    const auto one=ALKitMath::dragTopAbsolute(s,180.f,70.f);float fake_eased=0.f;ALKitMath::DragResult repeated;
    for(int i=0;i<1000;++i){fake_eased+=(one.yaw-fake_eased)*.01f;repeated=ALKitMath::dragTopAbsolute(s,180.f,70.f);}
    ensure("fake easing moved",fake_eased!=0.f);ensure_approximately_equals_range("fake easing cannot drift yaw",repeated.yaw,one.yaw,.0001f);ensure_approximately_equals_range("fake easing cannot drift pitch",repeated.pitch,one.pitch,.0001f);
    ensure_approximately_equals_range("rotated object mirror facing",ALKitMath::effYaw(20.f,5.f,true,73.f),121.f,.001f);
}

template<> template<> void alkitmath_object::test<18>()
{
    ALKitMath::DragStart s;s.radius=100.f;s.canvas_radius=100.f;s.cx=s.cy=100.f;s.x0=100.f;s.y0=0.f;s.t0=s.base_yaw0=170.f;s.p0=s.base_pitch0=0.f;ALKitMath::DragResult r;
    for(int i=0;i<1000;++i){const float a=2.f*ALKitMath::PI*i/999.f;r=ALKitMath::dragTopAbsolute(s,s.cx+s.radius*std::sin(a),s.cy-s.radius*std::cos(a));}
    ensure_approximately_equals_range("full turn returns periodic base",r.yaw,s.base_yaw0,.01f);
}
} // namespace tut
