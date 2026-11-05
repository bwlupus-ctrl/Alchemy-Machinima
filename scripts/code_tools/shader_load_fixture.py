"""Extract production C++ for a small native shader-cache fixture, never the viewer."""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile


def run(root):
    from shader_load_check import MGR, HEADER, GLSL, function, has_main, feature_fields
    from weather_cpu_check import compiler_command
    source = (root / MGR).read_text(encoding='utf-8')
    header = (root / HEADER).read_text(encoding='utf-8')
    scanner = function(source, 'shader_source_has_main(') + '\n' + function(source, 'shader_shared_source_fingerprint(')
    init = function(source, 'LLShaderMgr::readShaderSource(') + '\n' + function(source, 'LLShaderMgr::initShaderCache(')
    hashed = function((root / GLSL).read_text(encoding='utf-8'), 'LLGLSLShader::hash(')
    features = function(source, 'LLShaderMgr::hashShaderCacheFeatures(')
    feature_class = function((root / 'indra/llrender/llglslshader.h').read_text(encoding='utf-8'), 'class LLShaderFeatures') + ';'
    fields = feature_fields((root / 'indra/llrender/llglslshader.h').read_text(encoding='utf-8'))
    # Retain the production entry guards, replacing only the binary I/O tail
    # with a spy. This proves no partial key reaches either binary operation.
    binary_guards = ''
    for name, boundary, result in (
            ('loadCachedProgramBinary', '    glProgramParameteri', 'false'),
            ('saveCachedProgramBinary', '    ProgramBinaryData binary_info', 'true')):
        method = function(source, 'LLShaderMgr::' + name + '(')
        binary_guards += method[:method.index(boundary)] + '\n    ++binary_io; return ' + result + ';\n}\n'
    schema = re.search(r'SHADER_CACHE_SCHEMA\s*=\s*"([^"]+)"', header)[1]
    out = root / '.tmp/shader_load/native'
    out.mkdir(parents=True, exist_ok=True)
    code = r"""
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using F32=float;
using S32=int;
using U32=unsigned;
std::string llformat(const char*,int n){return std::to_string(n);}
using GLsizei=int;
using GLenum=unsigned;
namespace fs=std::filesystem;
struct LLUUID {
 std::string value;
 LLUUID()=default; LLUUID(const std::string& s):value(s){} LLUUID(const char* s):value(s){}
 bool notNull()const{return !value.empty();}std::string asString()const{return value;}
 bool operator==(const LLUUID& b)const{return value==b.value;}
 bool operator<(const LLUUID& b)const{return value<b.value;}
};
std::string last_hash_input;
struct HBXXH128 { // Test-only deterministic digest; production feeds/ordering unchanged.
 std::string input;
 uint64_t value=14695981039346656037ull;
 void update(const void* p,size_t n){input.append(static_cast<const char*>(p),n);auto b=static_cast<const unsigned char*>(p);while(n--){value^=*b++;value*=1099511628211ull;}}
 void update(const std::string& s){update(s.data(),s.size());}
 LLUUID digest(){last_hash_input=input;std::ostringstream s;s<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return LLUUID(s.str());}
};
struct fsyspath:fs::path {
 fsyspath(const char* s):fs::path(fs::u8path(s)){}
 fsyspath(const std::string& s):fs::path(fs::u8path(s)){}
 fsyspath(const fs::path& p):fs::path(p){}
 std::string string()const{return u8string();}
};
#define LL_PROFILE_ZONE_SCOPED
#define LL_INFOS(x) std::ostringstream()
#define LL_WARNS(x) std::ostringstream()
#define LL_ENDL std::endl
using llifstream=std::ifstream;
constexpr int LL_PATH_CACHE=0;
struct {double mGLVersion=4.6;std::string mGLVendor="vendor",mGLRenderer="renderer",mGLVersionString="4.6 test";}gGLManager;
struct Directory {std::string cache;std::string getDirDelimiter(){return "/";}
 std::string getExpandedFilename(int,const char*){return cache;}
 std::string add(const std::string& a,const std::string& b){return (fsyspath(a)/b).u8string();}
}directory,*gDirUtilp=&directory;
struct LLFile{static bool isfile(const std::string& p){return fs::is_regular_file(fsyspath(p));}static void mkdir(const std::string& path){fs::create_directories(fsyspath(path));}};
struct LLSD {bool map=false;std::string text;std::map<std::string,LLSD> entries;
 LLSD& operator[](const char* key){return entries[key];}
 const LLSD& operator[](const char* key)const{static const LLSD empty;auto it=entries.find(key);return it==entries.end()?empty:it->second;}bool isMap()const{return map;}
 LLUUID asUUID()const{return LLUUID(text);}int asInteger()const{return 123;}double asReal()const{return 0;}
};
bool parse_ok=true,metadata_map=true;
std::string metadata_schema="SCHEMA_VALUE";
struct LLSDSerialize {static constexpr int SIZE_UNLIMITED=-1;
 static int fromBinary(LLSD& data,std::istream&,int){if(!parse_ok)return -1;
 data.map=metadata_map;data["version"].text=metadata_schema;data["shaders"].map=metadata_map;
 data["shaders"].entries["program"]={};return 1;}};
namespace llsd {const auto& inMap(const LLSD& data){return data.entries;}}
struct LLGLSLShader;
std::string denied_source;
struct LLShaderMgr {
 static LLShaderMgr* instance();
 static constexpr U32 SHADER_CACHE_EPOCH=EPOCH_VALUE;
 static S32 shaderLanguageVersion(GLenum){return 420;}
 std::map<std::string,unsigned>mShaderObjects,mVertexShaderObjects,mFragmentShaderObjects;
 std::vector<std::string>mReservedAttribs;
 LLUUID mShaderCacheFeatureHash;
 void hashShaderCacheFeatures();
 void disableShaderCache(){mShaderCacheSourceValid=false;mShaderCacheEnabled=false;}
 static constexpr const char* SHADER_CACHE_SCHEMA="SCHEMA_VALUE";
 struct ProgramBinaryData{GLsizei mBinaryLength=0;GLenum mBinaryFormat=0;F32 mLastUsedTime=0;};
 std::map<LLUUID,ProgramBinaryData>mShaderBinaryCache;
 LLUUID mShaderCacheVersion,mSharedShaderFingerprint;
 bool mShaderCacheEnabled=false,mShaderCacheReadOnly=false,mShaderCacheScanAttempted=false,mShaderCacheSourceValid=false;
 std::string mShaderCacheDir,prefix;
 int purges=0;
 std::string getShaderDirPrefix(){return prefix;}
 void clearShaderCache(){++purges;mShaderBinaryCache.clear();}
 static bool readShaderSource(const std::string&,std::string&);
 static bool readShaderSourceActual(const std::string&,std::string&);
 int binary_io=0;
 bool loadCachedProgramBinary(LLGLSLShader*);
 bool saveCachedProgramBinary(LLGLSLShader*);
 void initShaderCache(bool,bool);
};
LLShaderMgr* active_manager=nullptr;
LLShaderMgr* LLShaderMgr::instance(){return active_manager;}
FEATURE_CLASS
struct LLGLSLShader {
 bool mShaderCacheEligible=true;
 std::string mName;int mShaderGroup=0,mShaderLevel=2;LLShaderFeatures mFeatures{};
 std::vector<std::pair<std::string,GLenum>>mShaderFiles;
 std::map<std::string,std::string>mDefines;
 inline static std::map<std::string,std::string>sGlobalDefines;
 inline static int sIndexedTextureChannels=4;
 LLUUID hash();
};
PADDED_SHADER
SCANNER
INIT
bool LLShaderMgr::readShaderSource(const std::string& path,std::string& content){
 return (denied_source.empty() || !fs::equivalent(fsyspath(path),fsyspath(denied_source))) && readShaderSourceActual(path,content);
}
FEATURES
HASH
PADDED_HASH
BINARY_GUARDS
static void write(const fs::path& path,const std::string& source){fs::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<source;}
static void require(bool result,const char* message){if(!result){std::cerr<<message<<"\n";std::exit(1);}}
int main(int argc,char** argv){
 if(argc==3 && std::string(argv[1])=="fingerprint"){
  const auto start=std::chrono::steady_clock::now();
  LLUUID fp;require(shader_shared_source_fingerprint(argv[2],fp),"actual shader tree scan failed");
  const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<fp.asString()<<"\n";
  std::cerr<<"native production full shader-tree scan (enumerate/read/classify/hash): "<<std::fixed<<std::setprecision(3)<<ms<<" ms\n";
  return 0;
 }
 if(argc==3 && std::string(argv[1])=="classify"){
  const auto tree=fsyspath(argv[2]);std::vector<fs::path> paths;
  for(const auto& entry:fs::recursive_directory_iterator(tree))
   if(entry.is_regular_file()&&entry.path().extension()==".glsl")paths.push_back(entry.path());
  std::sort(paths.begin(),paths.end());
  for(const auto& path:paths){
   std::string source;require(LLShaderMgr::readShaderSource(fsyspath(path).string(),source),"classification source read failed");
   std::cout<<(shader_source_has_main(source)?"root\t":"shared\t")<<fs::relative(path,tree).generic_u8string()<<"\n";
  }
  return 0;
 }
 require(argc==2,"fixture output path required");
 require(!shader_source_has_main("// void main() {}\nfloat utility();"),"main in line comment matched");
 require(!shader_source_has_main("/* void main() {} */\nfloat utility();"),"main in block comment matched");
 require(!shader_source_has_main("/*"+std::string(1024*1024+1,'x')+"\nmain(\n*/"),"main in >1 MiB block comment matched");
 require(!shader_source_has_main("mainImage(); domain(); _main(); 1main(); main_(); main1();"),"identifier boundary failure");
 require(shader_source_has_main("void main \t\r\n\f\v ( ) {}"),"main with spaces/newline missed");
 require(shader_source_has_main("void main/* mainImage() */ // line\n ( ) {}"),"comments between main and parenthesis missed");
 require(!shader_source_has_main("ma/* comment */in();"),"comment joined partial identifiers");
 require(!shader_source_has_main("/*"),"empty unterminated block matched");
 require(shader_source_has_main("/* unterminated main("),"legacy unterminated block main missed");
 require(!shader_source_has_main("main /* unterminated ("),"unterminated block treated as whitespace");
 require(!shader_source_has_main("/* unterminated // main(\nutility()"),"unterminated block line comment matched");
 require(shader_source_has_main("/* unterminated // hidden\nmain ("),"unterminated block after line comment missed");
 std::cout<<"native production classifier: comments, >1 MiB block, whole identifiers, whitespace and unterminated blocks PASS\n";
 fs::path base=fsyspath(argv[1]);
 const fs::path tree=base/fs::u8path(u8"unicode_\u00e9_\u955c")/"shaders";
 const auto prefix=(tree/"class").u8string();
 write(tree/"class1/lookF.glsl","void main() { }\n");
 write(tree/"class1/otherF.glsl","void main() { }\n");
 write(tree/"class1/util.glsl","// main() is only documentation\nfloat utility() {return 1.;}\n");
 write(tree/"class2/util.glsl","float utility() {return 2.;}\n");
 LLShaderMgr key_manager;key_manager.prefix=prefix;active_manager=&key_manager;
 key_manager.mReservedAttribs={"position","texcoord"};
 key_manager.mShaderObjects={{"compiled-feature-A",7}};
 key_manager.mFragmentShaderObjects={{"util.glsl",7}};key_manager.hashShaderCacheFeatures();
 require(shader_shared_source_fingerprint(prefix,key_manager.mSharedShaderFingerprint),"key scan failed");
 LLGLSLShader look_a,look_b;look_a.mName="look A";look_b.mName="look B";
 look_a.mShaderFiles={{"lookF.glsl",1}};look_b.mShaderFiles={{"otherF.glsl",1}};
 const auto a_before=look_a.hash(),b_before=look_b.hash();
 // Force a permitted layout with tail padding while preserving the exact
 // production fields and hash feeds. The current MSVC layout may have none.
 PaddedShader padded_a,padded_b;
 std::memset(&padded_a.mFeatures,0x55,sizeof(padded_a.mFeatures));
 std::memset(&padded_b.mFeatures,0xaa,sizeof(padded_b.mFeatures));
 SAME_FIELDS
 require(std::memcmp(&padded_a.mFeatures,&padded_b.mFeatures,sizeof(padded_a.mFeatures))!=0,"padding fixture has no distinct bytes");
 padded_a.hash();const auto input_a=last_hash_input;
 padded_b.hash();require(last_hash_input==input_a,"feature padding changed production hash input");
 std::cout<<"native production features: 30 fields; distinct padding bytes, identical key input PASS (forced alignas(64) layout; production size "<<sizeof(LLShaderFeatures)<<")\n";
 key_manager.mShaderCacheEnabled=true;
 const auto no_binary=[&](LLGLSLShader& shader){
  const auto calls=key_manager.binary_io;
  require(!key_manager.loadCachedProgramBinary(&shader),"partial key loaded binary");
  require(key_manager.saveCachedProgramBinary(&shader),"ineligible save must skip successfully");
  require(key_manager.binary_io==calls,"ineligible key reached binary I/O");
 };
 LLGLSLShader invalid;invalid.mShaderFiles={{"lookF.glsl",1}};invalid.mShaderLevel=0;
 invalid.hash();require(!invalid.mShaderCacheEligible&&key_manager.mShaderCacheEnabled,"level zero disabled other programs");no_binary(invalid);
 invalid.mShaderLevel=1;invalid.mShaderFiles={{"higherOnly.glsl",1}};
 write(tree/"class2/higherOnly.glsl","void main(){}\n");
 invalid.hash();require(!invalid.mShaderCacheEligible&&key_manager.mShaderCacheEnabled,"missing class root disabled other programs");no_binary(invalid);
 invalid.mShaderFiles={{"lookF.glsl",1}};denied_source=prefix+"1/lookF.glsl";
 invalid.hash();require(!invalid.mShaderCacheEligible&&key_manager.mShaderCacheEnabled,"unreadable root disabled other programs");no_binary(invalid);
 denied_source.clear();invalid.hash();require(invalid.mShaderCacheEligible,"recreated valid program retained stale rejection");
 look_b.hash();const auto io_before=key_manager.binary_io;
 key_manager.loadCachedProgramBinary(&look_b);key_manager.saveCachedProgramBinary(&look_b);
 require(key_manager.binary_io==io_before+2,"bad program stopped other program caching");
 const auto good_aliases=key_manager.mFragmentShaderObjects;
 key_manager.mFragmentShaderObjects={{"unmapped shared module",404}};
 key_manager.hashShaderCacheFeatures();look_b.hash();
 require(!key_manager.mShaderCacheEnabled,"incomplete shared assembly scan left cache enabled");no_binary(look_b);
 key_manager.mFragmentShaderObjects=good_aliases;key_manager.hashShaderCacheFeatures();
 key_manager.mShaderCacheEnabled=true;
 std::cout<<"native production cache guards: level 0, higher-class-only, unreadable root isolated; load/save skip partial/global-disabled keys; eligible neighbor keeps caching PASS\n";
 LLUUID before,after;
 require(shader_shared_source_fingerprint(prefix,before),"valid UTF-8 tree rejected");
 write(tree/"class1/lookF.glsl","void main() { int changed=1; }\n");
 require(shader_shared_source_fingerprint(prefix,after)&&before==after,"root edit changed shared fingerprint");
 require(!(look_a.hash()==a_before)&&look_b.hash()==b_before,"production root key failed locality");
 const auto a_after_root=look_a.hash();
 write(tree/"class2/util.glsl","float utility() {return 3.;}\n");
 require(shader_shared_source_fingerprint(prefix,after)&&!(before==after),"lower-class shared edit escaped fingerprint");
 key_manager.mSharedShaderFingerprint=after;
 require(!(look_a.hash()==a_after_root)&&!(look_b.hash()==b_before),"shared edit did not invalidate all production keys");
 const auto with_attributes=look_a.hash();
 std::reverse(key_manager.mReservedAttribs.begin(),key_manager.mReservedAttribs.end());
 require(!(look_a.hash()==with_attributes),"attribute binding order escaped key");
 const auto feature_before=key_manager.mShaderCacheFeatureHash;
 key_manager.mShaderObjects={{"compiled-feature-A",99}};key_manager.mFragmentShaderObjects={{"util.glsl",99}};
 key_manager.hashShaderCacheFeatures();require(feature_before==key_manager.mShaderCacheFeatureHash,"process-local handles changed feature key");
 const auto code_before=look_a.hash();
 key_manager.mShaderObjects={{"compiled-feature-B",99}};key_manager.hashShaderCacheFeatures();
 require(!(look_a.hash()==code_before),"assembled feature code escaped key");
 write(tree/"class2/lookF.glsl","void main(){int top=1;}\n");
 const auto fallback_before=look_a.hash();write(tree/"class1/lookF.glsl","void main(){int fallback=2;}\n");
 require(!(look_a.hash()==fallback_before),"lower-class root fallback escaped key");
 before=after;
 write(tree/"nested/newShared.glsl","float another() {return 4.;}\n");
 require(shader_shared_source_fingerprint(prefix,after)&&!(before==after),"nested shared file escaped fingerprint");
 require(!shader_shared_source_fingerprint((base/"missing/class").u8string(),after),"missing scan root accepted");
 before=after;
 write(tree/"nested/empty.glsl","");
 std::string empty="old content";
 require(LLShaderMgr::readShaderSource((tree/"nested/empty.glsl").u8string(),empty)&&empty.empty(),"empty read rejected or retained old content");
 require(shader_shared_source_fingerprint(prefix,after)&&!(before==after),"empty shared file missing from fingerprint");
 write(tree/"class1/emptyRoot.glsl","");invalid.mShaderFiles={{"emptyRoot.glsl",1}};
 invalid.hash();require(invalid.mShaderCacheEligible,"empty root rejected");
 fs::remove(tree/"nested/empty.glsl");
 directory.cache=(base/"cache").u8string();
 fs::create_directories(fsyspath(directory.cache));
 const fs::path meta=fsyspath(directory.cache)/"shaderdata.llsd";
 write(meta,"fixture parser payload");
 const auto check=[&](const char* label,bool readable,const std::string& schema,bool second,bool keep,int purges){
  parse_ok=readable;metadata_schema=schema;metadata_map=true;
  LLShaderMgr manager;manager.prefix=prefix;manager.initShaderCache(true,second);
  require(manager.mShaderCacheEnabled,"fixture did not enable cache");
  require((!manager.mShaderBinaryCache.empty())==keep,label);require(manager.purges==purges,label);
  require(manager.mShaderCacheReadOnly==second,"second-instance read-only state lost");
 };
 check("clean exit",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,true,0);
 check("crash before settings save",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,true,0);
 check("new viewer build without shader change",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,true,0);
 write(tree/"class1/lookF.glsl","void main(){int root_edit=2;}\n");
 check("one root edit",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,true,0);
 write(tree/"class1/util.glsl","float utility(){return 8.;}\n");
 check("one shared edit",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,true,0);
 check("schema change",true,"old-schema",false,false,1);
 check("second instance schema mismatch",true,"old-schema",true,false,0);
 check("second instance valid metadata",true,LLShaderMgr::SHADER_CACHE_SCHEMA,true,true,0);
 check("unreadable metadata",false,LLShaderMgr::SHADER_CACHE_SCHEMA,false,false,1);
 check("second instance unreadable metadata",false,"",true,false,0);
 metadata_map=false;parse_ok=true;metadata_schema=LLShaderMgr::SHADER_CACHE_SCHEMA;
 LLShaderMgr malformed;malformed.prefix=prefix;malformed.initShaderCache(true,false);require(malformed.purges==1,"non-map metadata accepted");
 fs::remove(meta);check("missing metadata",true,LLShaderMgr::SHADER_CACHE_SCHEMA,false,false,1);
 write(meta,"fixture parser payload");
 denied_source=(tree/"class1/util.glsl").u8string();
 LLShaderMgr failed;failed.prefix=prefix;failed.initShaderCache(true,false);
 require(!failed.mShaderCacheEnabled&&failed.purges==0&&failed.mShaderBinaryCache.empty(),"scan failure served old binaries");
 denied_source.clear();failed.initShaderCache(true,false);
 require(!failed.mShaderCacheEnabled,"scan failure was re-enabled later in same run");
 std::cout<<"native production init: 12 keep/purge cases PASS; scan failure remains disabled\n"
          <<"native production scan: UTF-8/sorted nested shared files, roots, empty files valid, missing/unreadable failures PASS\n"
          <<"native production hash: root locality, shared invalidation, lower-class fallback, attributes, feature code and handle independence PASS\n";
}
"""
    shader_shell = code[code.index('struct LLGLSLShader {'):code.index('PADDED_SHADER')]
    code = (code.replace('SCHEMA_VALUE', schema)
            .replace('EPOCH_VALUE', re.search(r'SHADER_CACHE_EPOCH\s*=\s*(\d+)', header)[1])
            .replace('FEATURE_CLASS', feature_class + '\n' + feature_class.replace('class LLShaderFeatures', 'class alignas(64) PaddedFeatures'))
            .replace('PADDED_SHADER', shader_shell.replace('LLGLSLShader', 'PaddedShader').replace('LLShaderFeatures', 'PaddedFeatures'))
            .replace('PADDED_HASH', hashed.replace('LLGLSLShader::hash', 'PaddedShader::hash'))
            .replace('SAME_FIELDS', '\n'.join(' padded_a.mFeatures.%s=padded_b.mFeatures.%s={};' % (name, name) for name in fields))
            .replace('BINARY_GUARDS', binary_guards)
            .replace('SCANNER', scanner).replace('INIT', init.replace('LLShaderMgr::readShaderSource(', 'LLShaderMgr::readShaderSourceActual(', 1))
            .replace('FEATURES', features).replace('HASH', hashed))
    path = out / 'shader_load_native.cpp'
    path.write_text(code, encoding='utf-8-sig')
    executable = compiler_command(out, [path], 'shader_load_native')
    with tempfile.TemporaryDirectory(prefix='fixture_', dir=out) as data:
        result = subprocess.run([str(executable), data], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    print(result.stdout, end='')
    # Compare production scan with an independently enumerated complete manifest.
    # FNV is substituted only for HBXXH; the real C++ scanner chooses every feed.
    value = 14695981039346656037
    manifest = []
    classifications = {}
    tree = root / 'indra/newview/app_settings/shaders'
    for file in sorted(tree.rglob('*.glsl'), key=lambda p: p.parts):
        content = file.read_bytes()
        relative = file.relative_to(tree).as_posix()
        classifications[relative] = 'root' if has_main(content.decode('utf-8')) else 'shared'
        if classifications[relative] == 'root':
            continue
        manifest.append(relative)
        feeds = (relative.encode(), len(content).to_bytes(8, 'little'), content)
        for feed in feeds:
            for byte in feed:
                value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    result = subprocess.run([str(executable), 'classify', str(tree)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    actual = {path: kind for kind, path in (line.split('\t', 1) for line in result.stdout.splitlines())}
    assert actual == classifications, 'production classifier differs from the independent legacy regex manifest'
    assert len(classifications) == 366 and len(manifest) == 56, 'reviewed 310 root/56 shared inventory changed'
    print('native production classifier matches every real file: 366 total, 310 roots, 56 shared (legacy regex reference)')
    result = subprocess.run([str(executable), 'fingerprint', str(tree / 'class')], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout.strip() == f'{value:016x}', 'production scanner omitted/reordered a shared file'
    print(result.stderr, end='')
    (out / 'shared_manifest.txt').write_text('\n'.join(manifest) + '\n', encoding='utf-8')
    print('native production scan matches all %d shared files in complete on-disk manifest' % len(manifest))
    # Model program locality separately from the production metadata decision.
    def key(name, roots, shared):
        return hashlib.sha256(name.encode() + roots[name] + shared).digest()
    roots = {'look-a': b'root A', 'look-b': b'root B'}
    shared = b'shared-source fingerprint'
    before = {name: key(name, roots, shared) for name in roots}
    roots['look-a'] += b' edit'
    assert key('look-a', roots, shared) != before['look-a']
    assert key('look-b', roots, shared) == before['look-b']
    assert all(key(name, roots, shared + b' edit') != before[name] for name in roots)
    print('program-key model: root edit isolates its consumers; shared edit invalidates all PASS')
