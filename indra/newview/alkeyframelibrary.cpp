#include "llviewerprecompiledheaders.h"
#include "alkeyframelibrary.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llfile.h"
#include "lldir.h"
#include "lldiriterator.h"
#include <cmath>
namespace
{
bool readDoc(const LLSD& value,KfDoc& doc,std::string& why,unsigned depth)
{
    if(depth>64) { why="Keyframe document nesting limit"; return false; }
    if(value.isUndefined()) doc={};
    else if(value.isBoolean()) doc=KfDoc(value.asBoolean());
    else if(value.isReal() || value.isInteger())
    {
        double number=value.asReal(); if(!std::isfinite(number)) { why="Nonfinite document number"; return false; }
        doc=KfDoc(number);
    }
    else if(value.isString()) doc=KfDoc(value.asString());
    else if(value.isArray())
    {
        KfDoc::Array out;
        for(const auto& v:llsd::inArray(value)) { KfDoc d; if(!readDoc(v,d,why,depth+1)) return false; out.push_back(std::move(d)); }
        doc=KfDoc(std::move(out));
    }
    else if(value.isMap())
    {
        KfDoc::Object out;
        for(auto i=value.beginMap();i!=value.endMap();++i) { KfDoc d; if(!readDoc(i->second,d,why,depth+1)) return false; out.emplace(i->first,std::move(d)); }
        doc=KfDoc(std::move(out));
    }
    else { why="Unsupported keyframe document type"; return false; }
    return true;
}
bool readFile(const std::string& path,LLSD& doc,std::string& reason)
{
    llifstream stream(path,std::ios::binary);
    if(!stream) { reason="Cannot open keyframe file"; return false; }
    stream.seekg(0,std::ios::end); auto size=stream.tellg();
    if(size<0 || size>16*1024*1024) { reason="Keyframe file exceeds 16 MiB"; return false; }
    stream.seekg(0);
    if(LLSDSerialize::fromXML(doc,stream)==LLSDParser::PARSE_FAILURE) { reason="Malformed keyframe file"; return false; }
    return true;
}
}
LLSD ALKeyframeLibrary::toLLSD(const KfDoc& doc)
{
    if(auto p=std::get_if<bool>(&doc.value)) return LLSD(*p);
    if(auto p=std::get_if<double>(&doc.value)) return LLSD(*p);
    if(auto p=std::get_if<std::string>(&doc.value)) return LLSD(*p);
    if(auto p=std::get_if<KfDoc::Array>(&doc.value)) { LLSD result=LLSD::emptyArray(); for(const auto& d:*p) result.append(toLLSD(d)); return result; }
    if(auto p=std::get_if<KfDoc::Object>(&doc.value)) { LLSD result=LLSD::emptyMap(); for(const auto& [k,d]:*p) result[k]=toLLSD(d); return result; }
    return LLSD();
}
bool ALKeyframeLibrary::fromLLSD(const LLSD& value,KfDoc& result,std::string& reason)
{
    KfDoc candidate; if(!readDoc(value,candidate,reason,0)) return false; result=std::move(candidate); return true;
}
bool ALKeyframeLibrary::loadDecoded(const std::string& file,ALKf::Sequence& result,std::string& reason)
{
    LLSD data; KfDoc doc;
    ALKf::Sequence candidate;
    if(!readFile(file,data,reason) || !fromLLSD(data,doc,reason) || !ALKf::decode(doc,candidate,reason)) return false;
    result=std::move(candidate); return true;
}
bool ALKeyframeLibrary::load(const std::string& file,ALKf::Sequence& result,std::string& reason)
{
    ALKf::Sequence candidate;
    if(!loadDecoded(file,candidate,reason)) return false;
    ALKf::rewriteRefs(candidate,[](ALKf::RefSlot& ref){ if(ref.kind==ALKf::RefKind::Clone) { ref.kind=ALKf::RefKind::Point; ref.id={}; } });
    result=std::move(candidate); return true;
}
bool ALKeyframeLibrary::saveVerified(const std::string& file,const ALKf::Sequence& sequence,std::string& reason)
{
    if(!ALKf::validate(sequence,reason)) return false;
    const auto expected=ALKf::encode(sequence);
    const std::string temp=file+".tmp";
    { llofstream out(temp,std::ios::binary|std::ios::trunc);
      LLSDSerialize::toPrettyXML(toLLSD(expected),out); out.flush();
      if(!out.good()) { reason="Keyframe write failed"; return false; } }
    LLSD read; KfDoc actual;
    if(!readFile(temp,read,reason) || !fromLLSD(read,actual,reason) || actual!=expected)
    { reason="Keyframe verification failed: "+reason; LLFile::remove(temp); return false; }
    // LLFile::rename is the viewer's replace-file primitive; named files are not touched until verification.
    if(LLFile::rename(temp,file)!=0) { reason="Keyframe rename failed"; LLFile::remove(temp); return false; }
    return true;
}
LLSD ALKeyframeLibrary::saveScene(const ALKf::Sequence& sequence)
{
    LLSD block; block["payload_version"]=1; block["sequence"]=toLLSD(ALKf::encode(sequence)); return block;
}
bool ALKeyframeLibrary::loadScene(const LLSD& scene,ALKf::Sequence& result,std::string& reason)
{
    if(!scene.has("keyframes")) return true;
    const auto& block=scene["keyframes"];
    if(!block.isMap() || !block["payload_version"].isInteger() || block["payload_version"].asInteger()!=1)
    { reason="Unsupported keyframe scene payload"; return false; }
    KfDoc doc;
    if(!fromLLSD(block["sequence"],doc,reason)) return false;
    ALKf::Sequence candidate;
    if(!ALKf::decode(doc,candidate,reason)) return false;
    // Clone IDs are reference-only. Never spawn on load; keep the saved fallback point.
    ALKf::rewriteRefs(candidate,[](ALKf::RefSlot& ref){ if(ref.kind==ALKf::RefKind::Clone) { ref.kind=ALKf::RefKind::Point; ref.id={}; } });
    result=std::move(candidate); return true;
}
std::string ALKeyframeLibrary::directory()
{
    const auto dir=gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,"director_keyframes"); LLFile::mkdir(dir); return dir;
}
std::string ALKeyframeLibrary::path(const std::string& name)
{
    if(name.empty() || name=="." || name==".." || name.find_first_of("/\\:")!=std::string::npos) return {};
    return directory()+gDirUtilp->getDirDelimiter()+LLDir::getScrubbedFileName(name)+".xml";
}
std::vector<std::string> ALKeyframeLibrary::names()
{
    std::vector<std::string> result; LLDirIterator it(directory(),"*.xml"); std::string file;
    while(it.next(file)) result.push_back(file.substr(0,file.size()-4));
    std::sort(result.begin(),result.end()); return result;
}
bool ALKeyframeLibrary::remove(const std::string& name)
{
    const auto file=path(name); return !file.empty() && LLFile::remove(file)==0;
}
