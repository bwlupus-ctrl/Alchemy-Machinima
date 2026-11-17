#include "alkeyframecore.h"
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace ALKf
{
namespace
{
using A=Doc::Array;
using O=Doc::Object;
Doc num(double n) {return Doc(n);}
template<class T> Doc en(T e) {return num(int(e));}
Doc vec(Vec3 v) {return Doc(A{num(v.x),num(v.y),num(v.z)});}
Doc quat(Quat q) {return Doc(A{num(q.w),num(q.x),num(q.y),num(q.z)});}
Doc bytes(const Id& id) {A a;for(auto b:id) a.push_back(num(b));return Doc(a);}
Doc ref(const RefSlot& r)
{
    return Doc(O{{"kind",en(r.kind)},{"id",bytes(r.id)},{"stableId",bytes(r.stableId)},
                 {"joint",num(r.joint)},{"fallback",vec(r.fallback)},{"label",Doc(r.label)}});
}
const O& obj(const Doc& d) {return std::get<O>(d.value);}
const A& arr(const Doc& d) {return std::get<A>(d.value);}
const Doc& field(const Doc& d,const char* name) {return obj(d).at(name);}
double number(const Doc& d)
{
    double v=std::get<double>(d.value);if(!std::isfinite(v)) throw std::runtime_error("nonfinite number");return v;
}
double get(const Doc& d,const char* name) {return number(field(d,name));}
std::string str(const Doc& d) {return std::get<std::string>(d.value);}
bool boolean(const Doc& d) {return std::get<bool>(d.value);}
template<class T> T integer(const Doc& d,double low,double high)
{
    double n=number(d);if(n<low || n>high || std::floor(n)!=n) throw std::runtime_error("integer range");return T(n);
}
template<class T> T enumeration(const Doc& d,int max) {return T(integer<int>(d,0,max));}
std::uint32_t u32(const Doc& d) {return integer<std::uint32_t>(d,0,4294967295.);}
std::uint64_t u64(const Doc& d)
{
    const auto text=str(d);std::uint64_t n=0;auto result=std::from_chars(text.data(),text.data()+text.size(),n);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size()) throw std::runtime_error("revision");return n;
}
Vec3 readVec(const Doc& d)
{
    const auto& a=arr(d);if(a.size()!=3) throw std::runtime_error("vec3 size");return {number(a[0]),number(a[1]),number(a[2])};
}
Quat readQuat(const Doc& d)
{
    const auto& a=arr(d);if(a.size()!=4) throw std::runtime_error("quat size");return {number(a[0]),number(a[1]),number(a[2]),number(a[3])};
}
Id readBytes(const Doc& d)
{
    const auto& a=arr(d);if(a.size()!=16) throw std::runtime_error("id size");Id out{};
    for(std::size_t i=0;i<16;++i) out[i]=integer<std::uint8_t>(a[i],0,255);return out;
}
RefSlot readRef(const Doc& d)
{
    return {enumeration<RefKind>(field(d,"kind"),4),readBytes(field(d,"id")),readBytes(field(d,"stableId")),
        integer<std::int32_t>(field(d,"joint"),-2147483648.,2147483647.),readVec(field(d,"fallback")),str(field(d,"label"))};
}
void checkDoc(const Doc& d,int depth,std::size_t& budget)
{
    if(depth>64 || budget++>10000000) throw std::runtime_error("document size/depth");
    if(auto p=std::get_if<double>(&d.value);p && !std::isfinite(*p)) throw std::runtime_error("payload nonfinite");
    if(auto a=std::get_if<A>(&d.value)) for(const auto& v:*a) checkDoc(v,depth+1,budget);
    if(auto o=std::get_if<O>(&d.value)) for(const auto& v:*o) checkDoc(v.second,depth+1,budget);
}
}
Doc encode(const Sequence& s)
{
    A tracks;
    for(const auto& t:s.tracks)
    {
        A schema,keys;
        for(const auto& c:t.schema) schema.push_back(Doc(O{{"name",Doc(c.name)},{"type",en(c.type)},{"interp",en(c.interp)},{"space",en(c.space)}}));
        for(const auto& k:t.keys)
        {
            A values;
            for(const auto& v:k.values)
            {
                if(auto p=std::get_if<double>(&v)) values.push_back(num(*p));
                if(auto p=std::get_if<Vec3>(&v)) values.push_back(vec(*p));
                if(auto p=std::get_if<Quat>(&v)) values.push_back(quat(*p));
                if(auto p=std::get_if<std::int64_t>(&v)) values.push_back(Doc(std::to_string(*p)));
            }
            keys.push_back(Doc(O{{"id",num(k.id)},{"hold",num(k.hold)},{"paceMode",en(k.out.mode)},{"pace",num(k.out.value)},
                {"values",Doc(values)},{"handleMode",en(k.handles.mode)},{"in",vec(k.handles.in)},{"out",vec(k.handles.out)},
                {"ease",Doc(k.ease)},{"aimMode",en(k.aim.mode)},{"target",ref(k.aim.target)}}));
        }
        tracks.push_back(Doc(O{{"id",num(t.id)},{"nextKeyId",num(t.nextKeyId)},{"uid",Doc(t.uid)},{"kind",Doc(t.kind)},
            {"schema",Doc(schema)},{"keys",Doc(keys)},{"path",en(t.settings.path)},{"evenSpeed",Doc(t.settings.evenSpeed)},
            {"easeEnds",Doc(t.settings.easeEnds)},{"total",num(t.settings.total)},{"easeSeconds",num(t.settings.easeSeconds)},
            {"lock",ref(t.lock)},{"payload",t.clientPayload},{"revision",Doc(std::to_string(t.revision))}}));
    }
    return Doc(O{{"version",num(s.formatVersion)},{"name",Doc(s.name)},{"uid",Doc(s.uid)},{"nextTrackId",num(s.nextTrackId)},
        {"loop",Doc(s.loop)},{"startDelay",num(s.startDelay)},{"revision",Doc(std::to_string(s.revision))},{"tracks",Doc(tracks)}});
}
bool decode(const Doc& d,Sequence& destination,std::string& why)
{
    try
    {
        std::size_t budget=0;checkDoc(d,0,budget);Sequence s;
        s.formatVersion=u32(field(d,"version"));s.name=str(field(d,"name"));s.uid=str(field(d,"uid"));
        s.nextTrackId=u32(field(d,"nextTrackId"));s.loop=boolean(field(d,"loop"));s.startDelay=get(d,"startDelay");s.revision=u64(field(d,"revision"));
        for(const auto& td:arr(field(d,"tracks")))
        {
            Track t;t.id=u32(field(td,"id"));t.nextKeyId=u32(field(td,"nextKeyId"));t.uid=str(field(td,"uid"));t.kind=str(field(td,"kind"));
            for(const auto& cd:arr(field(td,"schema"))) t.schema.push_back({str(field(cd,"name")),enumeration<ChannelType>(field(cd,"type"),3),
                enumeration<Interp>(field(cd,"interp"),2),enumeration<Space>(field(cd,"space"),2)});
            t.settings={enumeration<Path>(field(td,"path"),1),boolean(field(td,"evenSpeed")),boolean(field(td,"easeEnds")),get(td,"total"),get(td,"easeSeconds")};
            t.lock=readRef(field(td,"lock"));t.clientPayload=field(td,"payload");t.revision=u64(field(td,"revision"));
            for(const auto& kd:arr(field(td,"keys")))
            {
                Key k;k.id=u32(field(kd,"id"));k.hold=get(kd,"hold");k.out={enumeration<PaceMode>(field(kd,"paceMode"),1),get(kd,"pace")};
                k.handles={enumeration<HandleMode>(field(kd,"handleMode"),2),readVec(field(kd,"in")),readVec(field(kd,"out"))};
                k.ease=boolean(field(kd,"ease"));k.aim={enumeration<AimMode>(field(kd,"aimMode"),2),readRef(field(kd,"target"))};
                const auto& values=arr(field(kd,"values"));if(values.size()!=t.schema.size()) throw std::runtime_error("channel count");
                for(std::size_t j=0;j<values.size();++j)
                {
                    const auto& v=values[j];
                    switch(t.schema[j].type)
                    {
                    case ChannelType::Scalar:k.values.emplace_back(number(v));break;
                    case ChannelType::Vec3:k.values.emplace_back(readVec(v));break;
                    case ChannelType::Quat:k.values.emplace_back(readQuat(v));break;
                    case ChannelType::Step:
                    {
                        auto text=str(v);std::int64_t n=0;auto r=std::from_chars(text.data(),text.data()+text.size(),n);
                        if(r.ec!=std::errc{} || r.ptr!=text.data()+text.size()) throw std::runtime_error("step value");k.values.emplace_back(n);break;
                    }
                    }
                }
                t.keys.push_back(std::move(k));
            }
            s.tracks.push_back(std::move(t));
        }
        if(!validate(s,why)) return false;
        for(const auto& t:s.tracks) {Cache c;if(!buildCache(t,c,why)) return false;}
        destination=std::move(s);return true;
    }
    catch(const std::exception& e) {why=e.what();return false;}
}
}
