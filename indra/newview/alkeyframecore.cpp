#include "alkeyframecore.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace ALKf
{
namespace
{
double clamp(double v, double lo, double hi) { return std::clamp(v, lo, hi); }
double smooth(double x) { x = clamp(x, 0, 1); return x*x*(3-2*x); }
double smoother(double x) { x = clamp(x, 0, 1); return x*x*x*(x*(6*x-15)+10); }
bool number(double v) { return std::isfinite(v); }
}
Vec3 Vec3::operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
Vec3 Vec3::operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
Vec3 Vec3::operator-() const { return {-x,-y,-z}; }
Vec3 Vec3::operator*(double s) const { return {x*s,y*s,z*s}; }
Vec3 Vec3::operator/(double s) const { return {x/s,y/s,z/s}; }
double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double length(Vec3 v) { return std::sqrt(dot(v,v)); }
Vec3 unit(Vec3 v, Vec3 fallback) { double n=length(v); return n>1e-15 ? v/n : fallback; }
bool finite(Vec3 v) { return number(v.x)&&number(v.y)&&number(v.z); }
Quat Quat::operator*(Quat b) const
{
    return {w*b.w-x*b.x-y*b.y-z*b.z,w*b.x+x*b.w+y*b.z-z*b.y,
            w*b.y-x*b.z+y*b.w+z*b.x,w*b.z+x*b.y-y*b.x+z*b.w};
}
Quat Quat::operator-() const { return {-w,-x,-y,-z}; }
Quat Quat::inverse() const { return {w,-x,-y,-z}; }
Vec3 Quat::rotate(Vec3 v) const
{
    Vec3 q{x,y,z}; return v+cross(q,v)*(2*w)+cross(q,cross(q,v))*2;
}
double dot(Quat a, Quat b) { return a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z; }
Quat unit(Quat q)
{
    double n=std::sqrt(dot(q,q));
    return n>1e-15 ? Quat{q.w/n,q.x/n,q.y/n,q.z/n} : Quat{};
}
bool finite(Quat q) { return number(q.w)&&number(q.x)&&number(q.y)&&number(q.z); }
Quat exp(Vec3 v)
{
    double a=length(v), s=a>1e-12 ? std::sin(a)/a : 1-a*a/6;
    return unit(Quat{std::cos(a),v.x*s,v.y*s,v.z*s});
}
Vec3 log(Quat q)
{
    q=unit(q); if(q.w<0) q=-q;
    Vec3 v{q.x,q.y,q.z}; double n=length(v);
    return n>1e-15 ? v*(std::atan2(n,q.w)/n) : v;
}
Quat axisAngle(Vec3 axis, double a) { return exp(unit(axis)*(a/2)); }
Quat canonical(Quat q, Quat prev)
{
    q=unit(q); double d=dot(q,prev);
    if(d<0 || (d==0 && (q.w<0 || (q.w==0 && (q.x<0 || (q.x==0 && (q.y<0 || (q.y==0 && q.z<0)))))))) q=-q;
    return q;
}
Quat slerp(Quat a, Quat b, double u)
{
    b=canonical(b,a); return unit(a*exp(log(a.inverse()*b)*u));
}
double angle(Quat a, Quat b) { return 2*length(log(unit(a).inverse()*unit(b))); }
Quat shortestArc(Vec3 a, Vec3 b)
{
    a=unit(a); b=unit(b); double d=dot(a,b);
    if(d<-1+1e-12) return axisAngle(unit(cross(a,std::abs(a.z)<.9 ? Vec3{0,0,1}:Vec3{0,1,0})),pi);
    Vec3 c=cross(a,b); return unit(Quat{1+d,c.x,c.y,c.z});
}

Track cameraTrack()
{
    Track t;
    t.schema={{"position",ChannelType::Vec3,Interp::Smooth,Space::Linear},
              {"rotation",ChannelType::Quat,Interp::Smooth,Space::Linear},
              {"fov",ChannelType::Scalar,Interp::Smooth,Space::LogTan},
              {"roll",ChannelType::Scalar,Interp::Smooth,Space::Angle}};
    return t;
}
namespace
{
bool validRef(const RefSlot& r) { return int(r.kind)>=0 && int(r.kind)<=int(RefKind::Point) && finite(r.fallback); }
bool validTrack(const Track& t, std::string& why)
{
    auto fail=[&](const char* s){why=s; return false;};
    if(t.schema.empty() || t.schema.size()>64 || t.keys.size()>100000) return fail("track size");
    if(int(t.settings.path)<0 || int(t.settings.path)>1 || !number(t.settings.total) || t.settings.total<0 ||
       !number(t.settings.easeSeconds) || t.settings.easeSeconds<.1 || t.settings.easeSeconds>3) return fail("settings");
    if(!validRef(t.lock)) return fail("lock");
    std::set<std::string> names;
    for(const auto& c:t.schema)
        if(c.name.empty() || !names.insert(c.name).second || int(c.type)<0 || int(c.type)>3 ||
           int(c.interp)<0 || int(c.interp)>2 || int(c.space)<0 || int(c.space)>2 ||
           (c.type!=ChannelType::Scalar && c.space!=Space::Linear)) return fail("schema");
    if(t.kind=="camera")
    {
        for(const auto& c:cameraTrack().schema)
        {
            auto i=std::find_if(t.schema.begin(),t.schema.end(),[&](const Channel& x){return x.name==c.name && x.type==c.type && x.space==c.space;});
            if(i==t.schema.end()) return fail("camera schema");
        }
    }
    std::set<std::uint32_t> ids;
    for(const auto& k:t.keys)
    {
        if(!k.id || !ids.insert(k.id).second || k.id>=t.nextKeyId) return fail("key identity");
        if(!number(k.hold)||k.hold<0 || !number(k.out.value)||k.out.value<=0 || int(k.out.mode)<0 || int(k.out.mode)>1 ||
           (k.out.mode==PaceMode::Duration && k.out.value<.001)) return fail("key timing");
        if(k.values.size()!=t.schema.size() || !finite(k.handles.in)||!finite(k.handles.out) ||
           int(k.handles.mode)<0 || int(k.handles.mode)>2 || int(k.aim.mode)<0 || int(k.aim.mode)>2 || !validRef(k.aim.target)) return fail("key fields");
        for(std::size_t c=0;c<t.schema.size();++c)
        {
            const auto& v=k.values[c]; auto type=t.schema[c].type;
            if((type==ChannelType::Scalar && !std::holds_alternative<double>(v)) ||
               (type==ChannelType::Vec3 && !std::holds_alternative<Vec3>(v)) ||
               (type==ChannelType::Quat && !std::holds_alternative<Quat>(v)) ||
               (type==ChannelType::Step && !std::holds_alternative<std::int64_t>(v))) return fail("value type");
            if(auto p=std::get_if<double>(&v))
            {
                if(!number(*p) || (t.schema[c].space==Space::LogTan && !(*p>0 && *p<pi))) return fail("scalar finite/range");
            }
            if(auto p=std::get_if<Vec3>(&v); p && !finite(*p)) return fail("vector finite");
            if(auto p=std::get_if<Quat>(&v); p && (!finite(*p) || dot(*p,*p)<1e-20)) return fail("quaternion finite");
        }
    }
    return true;
}
}
bool validate(const Sequence& s, std::string& why)
{
    if(s.formatVersion!=1 || !number(s.startDelay)||s.startDelay<0 || !s.nextTrackId) {why="sequence header";return false;}
    std::set<std::uint32_t> ids; unsigned cameras=0;
    for(const auto& t:s.tracks)
    {
        if(!t.id || t.id>=s.nextTrackId || !ids.insert(t.id).second || (t.kind=="camera" && ++cameras>1)) {why="track identity/camera count";return false;}
        if(!validTrack(t,why)) return false;
    }
    return true;
}
std::uint32_t appendTrack(Sequence& s, Track t)
{
    if(!s.nextTrackId || s.nextTrackId==std::numeric_limits<std::uint32_t>::max()) return 0;
    if(t.kind=="camera" && std::any_of(s.tracks.begin(),s.tracks.end(),[](const Track& x){return x.kind=="camera";})) return 0;
    std::string why; if(!validTrack(t,why)) return 0;
    t.id=s.nextTrackId++; ++t.revision; s.tracks.push_back(std::move(t)); ++s.revision; return s.tracks.back().id;
}
std::uint32_t appendKey(Sequence& s, std::uint32_t id, Key key)
{
    auto i=std::find_if(s.tracks.begin(),s.tracks.end(),[&](const Track& t){return t.id==id;});
    if(i==s.tracks.end() || !i->nextKeyId || i->nextKeyId==std::numeric_limits<std::uint32_t>::max()) return 0;
    Track copy=*i; key.id=copy.nextKeyId++; copy.keys.push_back(std::move(key));
    Cache cache; std::string why; if(!buildCache(copy,cache,why)) return 0;
    ++copy.revision; *i=std::move(copy); ++s.revision; return i->keys.back().id;
}
bool eraseKey(Sequence& s, std::uint32_t tid, std::uint32_t kid)
{
    for(auto& t:s.tracks) if(t.id==tid)
    {
        auto k=std::find_if(t.keys.begin(),t.keys.end(),[&](const Key& x){return x.id==kid;});
        if(k==t.keys.end()) return false;
        Track copy=t; copy.keys.erase(copy.keys.begin()+(k-t.keys.begin()));
        Cache cache; std::string why; if(!buildCache(copy,cache,why)) return false;
        ++copy.revision; t=std::move(copy); ++s.revision; return true;
    }
    return false;
}

Vec3 Bezier::at(double u) const
{
    double v=1-u;return p[0]*(v*v*v)+p[1]*(3*v*v*u)+p[2]*(3*v*u*u)+p[3]*(u*u*u);
}
Vec3 Bezier::derivative(double u) const
{
    double v=1-u;return (p[1]-p[0])*(3*v*v)+(p[2]-p[1])*(6*v*u)+(p[3]-p[2])*(3*u*u);
}
namespace
{
double gauss(const Bezier& b, double a, double z)
{
    constexpr double x[]={0,.538469310105683091,.906179845938663993};
    constexpr double w[]={.568888888888888889,.478628670499366468,.236926885056189088};
    double h=(z-a)/2,m=(a+z)/2,sum=w[0]*length(b.derivative(m));
    for(int i=1;i<3;++i) sum+=w[i]*(length(b.derivative(m-x[i]*h))+length(b.derivative(m+x[i]*h)));
    return h*sum;
}
double integral(const Bezier& b,double a,double z,double tolerance=1e-10,int depth=0)
{
    double m=(a+z)/2, whole=gauss(b,a,z), split=gauss(b,a,m)+gauss(b,m,z);
    if(depth>=20 || std::abs(whole-split)<=tolerance) return split;
    return integral(b,a,m,tolerance/2,depth+1)+integral(b,m,z,tolerance/2,depth+1);
}
}
void Arc::build(Bezier b)
{
    curve=b; table.clear(); total=0; capped=false; table.push_back({0,0});
    for(auto p:b.p) if(!finite(p))
    {total=std::numeric_limits<double>::quiet_NaN();table.push_back({1,total});return;}
    // Adaptive quadrature error and a u-width bound both constrain each inverse bracket.
    std::function<void(double,double,int)> add=[&](double a,double z,int depth)
    {
        double m=(a+z)/2, whole=gauss(b,a,z), split=gauss(b,a,m)+gauss(b,m,z);
        if(table.size()<4095 && depth<20 && (z-a>1./32 || std::abs(whole-split)>1e-9*(z-a)))
        {add(a,m,depth+1);add(m,z,depth+1);return;}
        if(table.size()>=4095) capped=true;
        total+=integral(b,a,z); table.push_back({z,total});
    };
    add(0,1,0);
    // Depth-first subdivision can leave pending siblings at the cap; compact without losing length.
    if(table.size()>4096)
    {
        std::vector<ArcSample> compact; compact.reserve(4096);
        for(std::size_t i=0;i<4096;++i) compact.push_back(table[i*(table.size()-1)/4095]);
        table=std::move(compact); capped=true;
    }
}
double Arc::distance(double u) const
{
    if(u<=0 || total==0) return 0;
    if(u>=1) return total;
    auto i=std::upper_bound(table.begin(),table.end(),u,[](double x,const ArcSample& k){return x<k.u;})-1;
    return i->s+integral(curve,i->u,u);
}
double Arc::inverse(double s) const
{
    if(s<=0 || total==0) return 0;
    if(s>=total) return 1;
    auto hi=std::lower_bound(table.begin(),table.end(),s,[](const ArcSample& k,double x){return k.s<x;});
    auto lo=hi-1; double a=lo->u,b=hi->u;
    double u=a+(b-a)*(s-lo->s)/(hi->s-lo->s);
    for(int it=0;it<32;++it)
    {
        double error=lo->s+integral(curve,lo->u,u)-s;
        if(std::abs(error)<1e-11 || b-a<1e-9) return u;
        if(error>0) b=u; else a=u;
        double speed=length(curve.derivative(u));
        double next=speed>1e-14 ? u-error/speed : (a+b)/2;
        u=next>a && next<b ? next : (a+b)/2;
    }
    return (a+b)/2;
}
Timing durationTiming(double l,double t,unsigned n,double e)
{
    double a=std::min(e,t/4);return {t,a,l/(t-n*a/2)};
}
Timing speedTiming(double l,double c,unsigned n,double e)
{
    double candidate=l/c+n*e/2;
    if(e<=candidate/4) return {candidate,e,c};
    double t=(l/c)/(1-n/8.); return {t,t/4,c};
}
double pchip(double hl,double hr,double dl,double dr)
{
    if(dl*dr<=0) return 0;
    double w1=2*hr+hl,w2=hr+2*hl; return (w1+w2)/(w1/dl+w2/dr);
}
double hermite(double a,double b,double ma,double mb,double h,double u)
{
    double u2=u*u,u3=u2*u;
    return (2*u3-3*u2+1)*a+(u3-2*u2+u)*h*ma+(-2*u3+3*u2)*b+(u3-u2)*h*mb;
}
double hermiteSlope(double a,double b,double ma,double mb,double h,double u)
{
    return ((6*u*u-6*u)*a+(-6*u*u+6*u)*b)/h+(3*u*u-4*u+1)*ma+(3*u*u-2*u)*mb;
}
Quat RotationSegment::at(double u) const
{
    auto a=slerp(q[0],q[1],u),b=slerp(q[1],q[2],u),c=slerp(q[2],q[3],u);
    return slerp(slerp(a,b,u),slerp(b,c,u),u);
}
namespace
{
std::vector<Arc> spatial(const Track& t,std::size_t channel)
{
    const auto n=t.keys.size(); std::vector<Arc> result; if(n<2) return result;
    std::vector<Vec3> nodes; std::vector<std::size_t> node;
    for(const auto& k:t.keys)
    {
        Vec3 v=std::get<Vec3>(k.values[channel]);
        if(nodes.empty() || length(v-nodes.back())>.001) nodes.push_back(v);
        node.push_back(nodes.size()-1);
    }
    std::vector<Vec3> tangent(nodes.size());
    for(std::size_t j=1;j+1<nodes.size();++j)
    {
        double a=std::sqrt(length(nodes[j]-nodes[j-1])),b=std::sqrt(length(nodes[j+1]-nodes[j]));
        tangent[j]=(nodes[j]-nodes[j-1])/a-(nodes[j+1]-nodes[j-1])/(a+b)+(nodes[j+1]-nodes[j])/b;
    }
    for(std::size_t i=0;i+1<n;++i)
    {
        auto a=node[i],b=node[i+1]; Vec3 p=nodes[a],q=nodes[b],chord=q-p;
        Bezier curve{{p,p,q,q}};
        if(a==b) curve.p={p,p,p,p};
        else if(t.settings.path==Path::Straight) curve.p={p,p+chord/3,p+chord*(2./3),q};
        else
        {
            double dt=std::sqrt(length(chord));
            Vec3 out=a==0 ? chord/3:tangent[a]*(dt/3);
            Vec3 in=b+1==nodes.size() ? -chord/3:tangent[b]*(-dt/3);
            if(t.keys[i].handles.mode!=HandleMode::Auto) out=t.keys[i].handles.out;
            if(t.keys[i+1].handles.mode!=HandleMode::Auto) in=t.keys[i+1].handles.in;
            curve.p={p,p+out,q+in,q};
        }
        Arc arc; arc.build(curve); result.push_back(std::move(arc));
    }
    return result;
}
Vec3 endpointDirection(const Bezier& b,bool end)
{
    for(int j=1;j<4;++j)
    {
        Vec3 d=end ? b.p[3]-b.p[3-j]:b.p[j]-b.p[0];
        if(length(d)>1e-12) return unit(d);
    }
    return {};
}
std::size_t segment(const Cache& c,double t)
{
    auto it=std::upper_bound(c.arrival.begin(),c.arrival.end(),t);
    return it==c.arrival.begin() ? 0:std::min(std::size_t(it-c.arrival.begin()-1),c.arrival.size()-1);
}
double scalarTo(double v,Space s) { return s==Space::LogTan ? std::log(std::tan(v/2)):v; }
double scalarFrom(double v,Space s) { return s==Space::LogTan ? 2*std::atan(std::exp(v)):v; }
}
bool buildCache(const Track& t,Cache& destination,std::string& why)
{
    if(!validTrack(t,why)) return false;
    Cache c; c.revision=t.revision; auto n=t.keys.size(); c.channels.resize(t.schema.size());
    for(std::size_t j=0;j<t.schema.size();++j)
    {
        const auto& ch=t.schema[j];
        if(ch.name=="position" && ch.type==ChannelType::Vec3) c.position=int(j);
        if(ch.name=="rotation" && ch.type==ChannelType::Quat) c.rotation=int(j);
        if(ch.name=="fov" && ch.type==ChannelType::Scalar) c.fov=int(j);
        if(ch.name=="roll" && ch.type==ChannelType::Scalar) c.roll=int(j);
        if(ch.type==ChannelType::Vec3) c.channels[j].spatial=spatial(t,j);
        for(const auto& arc:c.channels[j].spatial) if(!number(arc.total)) {why="spatial finite";return false;}
    }
    if(!n) {destination=std::move(c);return true;}
    c.eased.resize(n); c.arrival.resize(n); c.departure.resize(n); c.distance.resize(n);
    std::vector<double> lengths(n-1,0);
    if(c.position>=0) for(std::size_t i=0;i+1<n;++i) lengths[i]=c.channels[c.position].spatial[i].total;
    for(std::size_t i=0;i<n;++i)
    {
        c.eased[i]=t.keys[i].ease || t.keys[i].hold>0 || (t.settings.easeEnds && (i==0 || i+1==n)) ||
            (i>0 && lengths[i-1]==0) || (i+1<n && lengths[i]==0);
        // A user corner or a straight-path corner cannot have nonzero continuous vector velocity.
        if(i>0 && i+1<n && c.position>=0 && lengths[i-1]>0 && lengths[i]>0)
        {
            auto& arcs=c.channels[c.position].spatial;
            if(dot(endpointDirection(arcs[i-1].curve,true),endpointDirection(arcs[i].curve,false))<1-1e-10) c.eased[i]=true;
        }
    }
    c.timing.resize(n-1);
    for(std::size_t i=0;i+1<n;++i)
    {
        auto pace=t.keys[i].out; unsigned eased=unsigned(c.eased[i])+unsigned(c.eased[i+1]);
        if(lengths[i]==0)
        {
            if(pace.mode!=PaceMode::Duration) {why="Static requires Duration";return false;}
            c.timing[i]={pace.value,0,0};
        }
        else c.timing[i]=pace.mode==PaceMode::Duration ? durationTiming(lengths[i],pace.value,eased,t.settings.easeSeconds):speedTiming(lengths[i],pace.value,eased,t.settings.easeSeconds);
    }
    if(t.settings.evenSpeed && n>1)
    {
        double r=t.settings.total,l=0;
        for(const auto& k:t.keys) r-=k.hold;
        for(std::size_t i=0;i+1<n;++i) if(lengths[i]==0) r-=c.timing[i].duration; else l+=lengths[i];
        if(!(r>0 && l>0)) {why="infeasible Even speed total";return false;}
        double low=l/r,high=(4./3)*l/r;
        for(int it=0;it<64;++it)
        {
            double speed=(low+high)/2,sum=0;
            for(std::size_t i=0;i+1<n;++i) if(lengths[i]>0) sum+=speedTiming(lengths[i],speed,unsigned(c.eased[i])+unsigned(c.eased[i+1]),t.settings.easeSeconds).duration;
            if(sum>r) low=speed; else high=speed;
        }
        for(std::size_t i=0;i+1<n;++i) if(lengths[i]>0) c.timing[i]=speedTiming(lengths[i],(low+high)/2,unsigned(c.eased[i])+unsigned(c.eased[i+1]),t.settings.easeSeconds);
    }
    for(std::size_t i=0;i<n;++i)
    {
        c.departure[i]=c.arrival[i]+t.keys[i].hold;
        if(i+1<n)
        {
            const auto& tm=c.timing[i];
            if(!number(tm.duration)||tm.duration<.001 || !number(tm.cruise)||tm.window>tm.duration/4+1e-12)
            {why="invalid timing solution";return false;}
            c.arrival[i+1]=c.departure[i]+tm.duration; c.distance[i+1]=c.distance[i]+lengths[i];
        }
    }
    c.end=c.departure.back();
    if(!number(c.end) || (t.settings.evenSpeed && n>1 && std::abs(c.end-t.settings.total)>1e-9*t.settings.total))
    {why="total constraint";return false;}
    // NaN slope marks an unconstrained key knot, solved ONCE from both incident pieces below.
    const double unspecified=std::numeric_limits<double>::quiet_NaN();
    auto add=[&](double time,double distance,double slope)
    {
        if(!c.knots.empty() && time==c.knots.back().t) {if(number(slope)) c.knots.back().slope=slope;return;}
        c.knots.push_back({time,distance,slope});
    };
    for(std::size_t i=0;i<n;++i)
    {
        add(c.arrival[i],c.distance[i],c.eased[i]?0:unspecified);
        if(t.keys[i].hold>0) add(c.departure[i],c.distance[i],0);
        if(i+1<n && lengths[i]>0)
        {
            const auto& tm=c.timing[i];
            if(c.eased[i]) add(c.departure[i]+tm.window,c.distance[i]+tm.cruise*tm.window/2,tm.cruise);
            if(c.eased[i+1]) add(c.arrival[i+1]-tm.window,c.distance[i+1]-tm.cruise*tm.window/2,tm.cruise);
        }
    }
    for(std::size_t i=0;i<c.knots.size();++i) if(!number(c.knots[i].slope))
    {
        auto sec=[&](std::size_t a,std::size_t b){return (c.knots[b].s-c.knots[a].s)/(c.knots[b].t-c.knots[a].t);};
        c.knots[i].slope=c.knots.size()==1 ? 0:i==0 ? sec(0,1):i+1==c.knots.size() ? sec(i-1,i):
            pchip(c.knots[i].t-c.knots[i-1].t,c.knots[i+1].t-c.knots[i].t,sec(i-1,i),sec(i,i+1));
    }
    for(std::size_t j=0;j<t.schema.size();++j)
    {
        auto& out=c.channels[j]; const auto& ch=t.schema[j];
        if(ch.type==ChannelType::Scalar)
        {
            out.scalar.resize(n);out.slope.resize(n,0);
            for(std::size_t i=0;i<n;++i)
            {
                double value=std::get<double>(t.keys[i].values[j]);
                if(ch.space==Space::Angle && i) value=out.scalar[i-1]+std::remainder(value-out.scalar[i-1],2*pi);
                out.scalar[i]=scalarTo(value,ch.space);
            }
            for(std::size_t i=0;i<n;++i) if(!c.eased[i] && n>1)
            {
                auto d=[&](std::size_t k){return (out.scalar[k+1]-out.scalar[k])/c.timing[k].duration;};
                out.slope[i]=i==0?d(0):i+1==n?d(i-1):pchip(c.timing[i-1].duration,c.timing[i].duration,d(i-1),d(i));
            }
        }
        if(ch.type==ChannelType::Quat)
        {
            out.quaternion.resize(n);
            for(std::size_t i=0;i<n;++i) out.quaternion[i]=canonical(std::get<Quat>(t.keys[i].values[j]),i?out.quaternion[i-1]:Quat{});
            std::vector<Vec3> omega(n); std::vector<double> delta(n-1);
            std::vector<bool> zero=c.eased;
            for(std::size_t i=0;i+1<n;++i)
            {
                delta[i]=angle(out.quaternion[i],out.quaternion[i+1]);
                if(delta[i]>170*degree) zero[i]=zero[i+1]=true;
            }
            for(std::size_t i=0;i<n;++i) if(!zero[i] && n>1)
            {
                Vec3 left=i ? log(out.quaternion[i-1].inverse()*out.quaternion[i])*2:Vec3{};
                Vec3 right=i+1<n ? log(out.quaternion[i].inverse()*out.quaternion[i+1])*2:Vec3{};
                if(i==0) omega[i]=right/c.timing[0].duration;
                else if(i+1==n) omega[i]=left/c.timing[i-1].duration;
                else
                {
                    double a=c.timing[i-1].duration,b=c.timing[i].duration;
                    omega[i]=(left*(b/a)+right*(a/b))/(a+b);
                }
                double bound=std::numeric_limits<double>::infinity();
                for(std::size_t k=(i?i-1:0);k<std::min(i+1,n-1);++k)
                    if(delta[k]<=170*degree) bound=std::min(bound,(3/c.timing[k].duration)*std::min(pi/4,(175*degree-delta[k])/2));
                double magnitude=length(omega[i]); if(magnitude>bound) omega[i]=omega[i]*(bound/magnitude);
            }
            for(std::size_t i=0;i+1<n;++i)
            {
                double duration=c.timing[i].duration;
                RotationSegment r{{out.quaternion[i],out.quaternion[i]*exp(omega[i]*(duration/6)),
                    out.quaternion[i+1]*exp(omega[i+1]*(-duration/6)),out.quaternion[i+1]},delta[i]>170*degree};
                for(int k=1;k<4;++k) r.q[k]=canonical(r.q[k],r.q[k-1]);
                out.rotation.push_back(r);
            }
        }
    }
    for(const auto& ch:c.channels) for(const auto& arc:ch.spatial)
        if(!number(arc.total)) {why="spatial finite";return false;}
    destination=std::move(c);return true;
}
namespace
{
double distanceEval(const Cache& c,double t,bool derivative)
{
    if(c.knots.empty()) return 0;
    if(c.knots.size()==1 || t<c.knots.front().t || t>c.knots.back().t) return derivative?0:(t<c.knots.front().t?c.knots.front().s:c.knots.back().s);
    auto it=std::upper_bound(c.knots.begin(),c.knots.end(),t,[](double x,const Knot& k){return x<k.t;});
    auto j=std::min(std::size_t(it-c.knots.begin()),c.knots.size()-1); if(!j) j=1;
    auto a=c.knots[j-1],b=c.knots[j];double h=b.t-a.t,u=(t-a.t)/h;
    return derivative?hermiteSlope(a.s,b.s,a.slope,b.slope,h,u):hermite(a.s,b.s,a.slope,b.slope,h,u);
}
}
double Cache::distanceAt(double t) const {return distanceEval(*this,t,false);}
double Cache::speedAt(double t) const {return distanceEval(*this,t,true);}
bool editTotal(Track& t,double total,std::string& why)
{
    double hold=0;for(const auto& key:t.keys) hold+=key.hold;
    if(!number(total) || total<=hold || t.keys.size()<2) {why="infeasible total";return false;}
    if(t.settings.evenSpeed)
    {
        Track copy=t;copy.settings.total=total;Cache c;if(!buildCache(copy,c,why)) return false;
        ++copy.revision;t=std::move(copy);return true;
    }
    auto scaled=[&](double k)
    {
        Track copy=t;
        for(std::size_t i=0;i+1<copy.keys.size();++i)
            copy.keys[i].out.value=t.keys[i].out.mode==PaceMode::Duration?t.keys[i].out.value*k:t.keys[i].out.value/k;
        return copy;
    };
    // Work below the 1ms validation boundary with the closed form, then validate the final edit.
    Cache original;if(!buildCache(t,original,why)) return false;
    auto end=[&](double k)
    {
        double result=hold;
        for(std::size_t i=0;i+1<t.keys.size();++i)
        {
            auto pace=t.keys[i].out;
            result+=pace.mode==PaceMode::Duration?pace.value*k:
                speedTiming(original.distance[i+1]-original.distance[i],pace.value/k,
                            unsigned(original.eased[i])+unsigned(original.eased[i+1]),t.settings.easeSeconds).duration;
        }
        return result;
    };
    double low=0,high=1;for(int i=0;i<100 && end(high)<total;++i) high*=2;
    if(end(high)<total) {why="total bracket";return false;}
    for(int i=0;i<64;++i) {double mid=(low+high)/2;if(end(mid)<total) low=mid;else high=mid;}
    Track copy=scaled((low+high)/2);copy.settings.total=total;Cache c;
    if(!buildCache(copy,c,why) || std::abs(c.end-total)>1e-9*total) {why="infeasible total at minimum duration";return false;}
    ++copy.revision;t=std::move(copy);return true;
}
double sequenceEnd(const std::vector<Cache>& caches)
{
    double end=0;for(const auto& c:caches) end=std::max(end,c.end);return end;
}
Value evalChannel(const Track& t,const Cache& c,std::size_t channel,double time)
{
    if(t.keys.empty() || channel>=t.schema.size() || c.revision!=t.revision) return 0.;
    std::size_t i=segment(c,time);const auto& ch=t.schema[channel];const auto& data=c.channels[channel];
    if(i+1==t.keys.size() || time<=c.departure[i] || ch.interp==Interp::Step || ch.type==ChannelType::Step)
    {
        if(ch.type==ChannelType::Scalar) return scalarFrom(data.scalar[i],ch.space);
        if(ch.type==ChannelType::Quat) return data.quaternion[i];
        if(ch.type==ChannelType::Vec3 && !data.spatial.empty()) return i<data.spatial.size()?data.spatial[i].curve.p[0]:data.spatial.back().curve.p[3];
        return t.keys[i].values[channel];
    }
    double duration=c.timing[i].duration,u=clamp((time-c.departure[i])/duration,0,1);
    if(ch.type==ChannelType::Scalar)
    {
        double v=ch.interp==Interp::Linear ? data.scalar[i]+(data.scalar[i+1]-data.scalar[i])*u:
            hermite(data.scalar[i],data.scalar[i+1],data.slope[i],data.slope[i+1],duration,u);
        return scalarFrom(v,ch.space);
    }
    if(ch.type==ChannelType::Quat) return ch.interp==Interp::Linear?slerp(data.quaternion[i],data.quaternion[i+1],u):data.rotation[i].at(u);
    const auto& arc=data.spatial[i];
    if(ch.interp==Interp::Linear) return arc.curve.p[0]+(arc.curve.p[3]-arc.curve.p[0])*u;
    // Camera position follows physical distance. Other vector channels share the segment clock.
    double s=int(channel)==c.position ? c.distanceAt(time)-c.distance[i] : arc.total*u;
    return arc.curve.at(arc.inverse(s));
}

Vec3 Smoother::update(Vec3 sample,bool found,double dt)
{
    if(!found) {present=false;return filtered;}
    if(!initialized) {initialized=true;present=true;previous=filtered=sample;return filtered;}
    bool continuous=present;present=true;
    if(!(dt>0) || !number(dt)) {previous=sample;return filtered;}
    if(continuous && dt>.5) {filtered=previous=sample;return filtered;}
    Vec3 start=continuous?previous:sample;
    constexpr double tau=.15;
    Vec3 v=(sample-start)/dt;
    filtered=sample-v*tau+(filtered-start+v*tau)*std::exp(-dt/tau);
    previous=sample;return filtered;
}
Quat levelAim(Vec3 f,double roll)
{
    f=unit(f);Vec3 y=unit(cross(Vec3{0,0,1},f),{0,1,0}),z=cross(f,y);
    // Rotation matrix columns are the +X forward, +Y left and +Z up axes.
    double trace=f.x+y.y+z.z;Quat q;
    if(trace>0) {double s=std::sqrt(trace+1)*2;q={s/4,(y.z-z.y)/s,(z.x-f.z)/s,(f.y-y.x)/s};}
    else if(f.x>y.y && f.x>z.z) {double s=std::sqrt(1+f.x-y.y-z.z)*2;q={(y.z-z.y)/s,s/4,(y.x+f.y)/s,(z.x+f.z)/s};}
    else if(y.y>z.z) {double s=std::sqrt(1+y.y-f.x-z.z)*2;q={(z.x-f.z)/s,(y.x+f.y)/s,s/4,(z.y+y.z)/s};}
    else {double s=std::sqrt(1+z.z-f.x-y.y)*2;q={(f.y-y.x)/s,(z.x+f.z)/s,(z.y+y.z)/s,s/4};}
    return unit(axisAngle(f,roll)*unit(q));
}
Quat previewAim(Vec3 camera,Vec3 target,Quat laid,double roll)
{
    Vec3 delta=target-camera;double d=length(delta);if(d<=.01) return laid;
    Vec3 f=unit(delta);
    Quat aim=std::abs(f.z)<std::cos(10*degree)?levelAim(f,roll):shortestArc(laid.rotate({1,0,0}),f)*laid;
    return slerp(laid,aim,smooth((d-.01)/.04));
}
Quat AimState::update(Vec3 forward,double roll,double dt)
{
    Vec3 f=unit(forward); integratedTime=0;
    if(!initialized) {presented=levelAim(f,roll);initialized=true;return presented;}
    Vec3 old=presented.rotate({1,0,0});
    if(!(dt>0) || !number(dt)) return presented;
    if(dt>1) {if(std::abs(f.z)<std::cos(10*degree)) presented=levelAim(f,roll);return presented;}
    constexpr double hmax=1./240;
    int n=int(std::ceil(dt/hmax)); double h=dt/n;
    Quat transport=shortestArc(old,f);
    // A skipped near-target interval (or a target jump) must make progress,
    // rather than reject the same opposite-facing sample forever.
    double turn=angle(transport,Quat{});
    if(turn>720*degree*dt) transport=slerp(Quat{},transport,720*degree*dt/turn);
    for(int j=1;j<=n;++j)
    {
        Vec3 step=slerp(Quat{},transport,double(j)/n).rotate(old);
        presented=unit(shortestArc(presented.rotate({1,0,0}),step)*presented);
        double p=std::abs(step.z);
        if(p<std::cos(degree))
        {
            Vec3 up=presented.rotate({0,0,1}),desired=levelAim(step,roll).rotate({0,0,1});
            double sine=dot(step,cross(up,desired)),cosine=dot(up,desired);
            // The 180-degree roll tie after a pole crossing must choose the same direction
            // at every presentation rate; signed roundoff in atan2 otherwise chooses either.
            double error=cosine<0 && std::abs(sine)<1e-12 ? pi:std::atan2(sine,cosine);
            double w=smooth((std::cos(10*degree)-p)/(std::cos(10*degree)-std::cos(35*degree)));
            double rate=(30+690*w)*degree;
            presented=unit(axisAngle(step,clamp(error,-rate*h,rate*h))*presented);
        }
        integratedTime+=h;
    }
    return presented;
}
Pose evalCamera(const Track& t,const Cache& c,double time,const Resolver& resolver,EvalMode mode,ClientRuntime& runtime)
{
    Pose pose;if(t.keys.empty() || c.revision!=t.revision || c.position<0 || c.rotation<0 || c.fov<0 || c.roll<0) return pose;
    pose.position=std::get<Vec3>(evalChannel(t,c,c.position,time));
    pose.rotation=std::get<Quat>(evalChannel(t,c,c.rotation,time));
    pose.fov=std::get<double>(evalChannel(t,c,c.fov,time));pose.roll=std::get<double>(evalChannel(t,c,c.roll,time));
    std::size_t i=segment(c,time),next=std::min(i+1,t.keys.size()-1);
    double u=i==next || time<=c.departure[i]?0:clamp((time-c.departure[i])/c.timing[i].duration,0,1);
    std::array<const RefSlot*,2> refs{};
    for(int j=0;j<2;++j)
    {
        const auto& aim=t.keys[j?next:i].aim;
        if(aim.mode==AimMode::OwnTarget) refs[j]=&aim.target;
        if(aim.mode==AimMode::FollowLock) refs[j]=&t.lock;
        if(refs[j] && refs[j]->kind==RefKind::None) refs[j]=nullptr;
    }
    std::array<int,2> slot{-1,-1};
    // Match ALL existing identities before allocating new slots: A/B -> B/C preserves B.
    for(int j=0;j<2;++j) if(refs[j])
        for(int k=0;k<2;++k) if(runtime.sources[k].assigned && runtime.sources[k].ref==RefIdentity(*refs[j])) {slot[j]=k;break;}
    for(int j=0;j<2;++j) if(refs[j] && slot[j]<0)
    {
        if(j==1 && refs[0] && RefIdentity(*refs[0])==RefIdentity(*refs[1])) {slot[1]=slot[0];continue;}
        int free=slot[1-j]==0?1:0;slot[j]=free;
        runtime.sources[free]={};runtime.sources[free].assigned=true;runtime.sources[free].ref=RefIdentity(*refs[j]);
    }
    std::array<Quat,2> sources{pose.rotation,pose.rotation};
    std::array<bool,2> evaluated{};
    std::array<Quat,2> aimed{};
    for(int j=0;j<2;++j) if(refs[j])
    {
        int k=slot[j];
        if(evaluated[k]) {sources[j]=aimed[k];continue;}
        auto& state=runtime.sources[k]; Vec3 target=refs[j]->fallback;
        bool found=refs[j]->kind==RefKind::Point || resolver.resolve(*refs[j],target);
        if(mode==EvalMode::Play)
        {
            if(!state.smoother.initialized) {state.smoother.filtered=state.smoother.previous=target;state.smoother.initialized=true;state.smoother.present=found;}
            target=state.smoother.update(target,found,runtime.wallDelta);
            Quat preview=previewAim(pose.position,target,pose.rotation,pose.roll);
            bool hadPresented=state.aim.initialized;Quat lastPresented=state.aim.presented;
            if(!state.aim.initialized) {state.aim.presented=preview;state.aim.initialized=true;}
            double distance=length(target-pose.position);
            if(distance<=.01) sources[j]=pose.rotation;
            else
            {
                // Blend the desired forward BEFORE transport. Crossing the target cannot leave
                // a stale opposite-facing state permanently caught by the teleport guard.
                Vec3 forward=preview.rotate({1,0,0});
                Vec3 level=levelAim(forward,0).rotate({0,0,1}),up=preview.rotate({0,0,1});
                double roll=distance>=.05?pose.roll:std::atan2(dot(forward,cross(level,up)),dot(level,up));
                sources[j]=state.aim.update(forward,roll,runtime.wallDelta);
                if(distance<.05) sources[j]=slerp(pose.rotation,sources[j],smooth((distance-.01)/.04));
            }
            // Forward transport and roll correction each carry a 720 deg/s ceiling; the laid-pose
            // assignment inside 1 cm and the near-target blend can both present
            // an opposite orientation in one frame. Bound the FINAL presented result.
            if(hadPresented && runtime.wallDelta>0 && runtime.wallDelta<=1 && number(runtime.wallDelta))
            {
                double turn=angle(lastPresented,sources[j]),most=1440*degree*runtime.wallDelta;
                if(turn>most) sources[j]=unit(slerp(lastPresented,sources[j],most/turn));
            }
            state.aim.presented=sources[j];
        }
        else sources[j]=previewAim(pose.position,target,pose.rotation,pose.roll);
        evaluated[k]=true;aimed[k]=sources[j];
    }
    pose.rotation=slerp(sources[0],sources[1],smoother(u));return pose;
}

void rewriteRefs(Sequence& s,const std::function<void(RefSlot&)>& rewrite)
{
    for(auto& t:s.tracks) {rewrite(t.lock);for(auto& k:t.keys) rewrite(k.aim.target);++t.revision;} ++s.revision;
}
void History::commit(const Sequence& before,const Sequence& after,std::string label,std::uint64_t seq)
{
    if(!seq || before==after) return;
    observe(before);observe(after);mExpectedRevision=after.revision;
    if(!mUndo.empty() && mUndo.back().seq==seq && mUndo.back().after->revision==before.revision)
    {
        mUndo.back().after=std::make_shared<const Sequence>(after);
        mRedo.clear();mGesture.reset();return;
    }
    mUndo.push_back({std::make_shared<const Sequence>(before),std::make_shared<const Sequence>(after),std::move(label),seq});
    if(mUndo.size()>100) mUndo.erase(mUndo.begin());mRedo.clear();mGesture.reset();
}
std::uint64_t History::sequence(bool redo) const {const auto& s=redo?mRedo:mUndo;return s.empty()?0:s.back().seq;}
bool History::ready(bool redo,std::uint64_t seq,const Sequence& live,bool owns) const
{
    const auto& s=redo?mRedo:mUndo;
    return !owns && !s.empty() && s.back().seq==seq && live.revision==mExpectedRevision;
}
bool History::replay(bool redo,Sequence& live,std::uint64_t replaySeq)
{
    if(!ready(redo,sequence(redo),live,false)) return false;
    auto& from=redo?mRedo:mUndo;auto& to=redo?mUndo:mRedo;
    auto step=from.back();observe(live);live=*(redo?step.after:step.before);observe(live);
    live.nextTrackId=mNextTrackId;
    for(auto& t:live.tracks) {t.nextKeyId=mNextKeys[t.id];t.revision=++mRevisionClock;}
    live.revision=++mRevisionClock;mExpectedRevision=live.revision;
    if(replaySeq) step.seq=replaySeq;
    from.pop_back();to.push_back(std::move(step));return true;
}
void History::discardRedo() {mRedo.clear();}
void History::discardStep(bool redo,std::uint64_t seq)
{
    auto& s=redo?mRedo:mUndo;if(!s.empty() && s.back().seq==seq) s.pop_back();
}
void History::beginGesture(const Sequence& s) {mGesture=std::make_shared<const Sequence>(s);}
bool History::cancelGesture(Sequence& s) {if(!mGesture) return false;s=*mGesture;mGesture.reset();return true;}
void History::endGesture() {mGesture.reset();}
void History::rekey(Sequence& live,const std::function<void(RefSlot&)>& rewrite)
{
    bool current=live.revision==mExpectedRevision;
    bool gestureCurrent=mGesture && mGesture->revision==mExpectedRevision;
    auto transform=[&](std::shared_ptr<const Sequence>& snapshot)
    {auto copy=std::make_shared<Sequence>(*snapshot);rewriteRefs(*copy,rewrite);observe(*copy);snapshot=std::move(copy);};
    rewriteRefs(live,rewrite);
    observe(live);if(current) mExpectedRevision=live.revision;
    for(auto* stack:{&mUndo,&mRedo}) for(auto& step:*stack) {transform(step.before);transform(step.after);}
    if(mGesture) transform(mGesture);
    if(gestureCurrent) mExpectedRevision=mGesture->revision;
}
std::size_t History::size(bool redo) const {return (redo?mRedo:mUndo).size();}
void History::observe(const Sequence& s)
{
    mRevisionClock=std::max(mRevisionClock,s.revision);mNextTrackId=std::max(mNextTrackId,s.nextTrackId);
    for(const auto& t:s.tracks) {mNextKeys[t.id]=std::max(mNextKeys[t.id],t.nextKeyId);mRevisionClock=std::max(mRevisionClock,t.revision);}
}
} // namespace ALKf
