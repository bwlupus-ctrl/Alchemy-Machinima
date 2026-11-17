#ifndef AL_KEYFRAME_LIBRARY_H
#define AL_KEYFRAME_LIBRARY_H
#include "alkeyframecore.h"
class LLSD;
namespace ALKeyframeLibrary
{
std::string directory();
std::string path(const std::string& name);
std::vector<std::string> names();
bool remove(const std::string& name);
LLSD toLLSD(const KfDoc&);
bool fromLLSD(const LLSD&, KfDoc&, std::string& reason);
bool saveVerified(const std::string& file, const ALKf::Sequence&, std::string& reason);
bool load(const std::string& file, ALKf::Sequence&, std::string& reason);
bool loadDecoded(const std::string& file, ALKf::Sequence&, std::string& reason); // as saved: no load-time reference rewriting
bool loadScene(const LLSD&, ALKf::Sequence&, std::string& reason);
LLSD saveScene(const ALKf::Sequence&);
}
#endif
