#include "SceneDocument.hpp"
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

namespace {
std::wstring makeId() {
    GUID g{}; if (FAILED(CoCreateGuid(&g))) return std::to_wstring(GetTickCount64());
    wchar_t b[40]{}; StringFromGUID2(g,b,40);
    std::wstring s=b; s.erase(std::remove(s.begin(),s.end(),L'{'),s.end());
    s.erase(std::remove(s.begin(),s.end(),L'}'),s.end()); return s;
}
std::string utf8(const std::wstring& s) {
    if(s.empty()) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    std::string r(n,'\0'); WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr); return r;
}
std::wstring wide(const std::string& s) {
    if(s.empty()) return {};
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);
    if(!n) return {};
    std::wstring r(n,L'\0'); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),r.data(),n); return r;
}
std::string quote(const std::wstring& value) {
    std::string s=utf8(value), out="\"";
    for(unsigned char c:s) { switch(c) { case '\\':out+="\\\\";break; case '"':out+="\\\"";break; case '\n':out+="\\n";break; case '\r':out+="\\r";break; case '\t':out+="\\t";break; default: if(c<0x20){char b[7];sprintf_s(b,"\\u%04x",c);out+=b;}else out+=(char)c; } }
    return out+"\"";
}
struct Json {
    enum Kind { Null, String, Number, Array, Object } kind=Null;
    std::string str; double number=0; std::vector<Json> array; std::unordered_map<std::string,Json> object;
};
class Parser {
public:
    explicit Parser(const std::string& s):s_(s){}
    bool parse(Json& v){skip(); if(!value(v))return false;skip();return p_==s_.size();}
private:
    void skip(){while(p_<s_.size() && (s_[p_]==' '||s_[p_]=='\n'||s_[p_]=='\r'||s_[p_]=='\t'))++p_;}
    bool string(std::string& out){
        if(p_>=s_.size()||s_[p_++]!='"')return false;
        while(p_<s_.size()){char c=s_[p_++];if(c=='"')return true;if((unsigned char)c<0x20)return false;
            if(c=='\\'){if(p_>=s_.size())return false;char e=s_[p_++];switch(e){case '"':case '\\':case '/':out+=e;break;case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;case 'u':{if(p_+4>s_.size())return false;unsigned v=0;for(int i=0;i<4;++i){char h=s_[p_++];v<<=4;if(h>='0'&&h<='9')v+=h-'0';else if(h>='a'&&h<='f')v+=h-'a'+10;else if(h>='A'&&h<='F')v+=h-'A'+10;else return false;} if(v<128)out+=(char)v;else if(v<2048){out+=(char)(0xC0|(v>>6));out+=(char)(0x80|(v&63));}else{out+=(char)(0xE0|(v>>12));out+=(char)(0x80|((v>>6)&63));out+=(char)(0x80|(v&63));}break;}default:return false;}}
            else out+=c;
        }return false;
    }
    bool value(Json& v){skip();if(p_>=s_.size())return false;char c=s_[p_];
        if(c=='"'){v.kind=Json::String;return string(v.str);}
        if(c=='{'){v.kind=Json::Object;++p_;skip();if(p_<s_.size()&&s_[p_]=='}'){++p_;return true;}for(;;){skip();std::string k;if(!string(k))return false;skip();if(p_>=s_.size()||s_[p_++]!=':')return false;Json x;if(!value(x))return false;v.object.emplace(std::move(k),std::move(x));skip();if(p_>=s_.size())return false;char d=s_[p_++];if(d=='}')return true;if(d!=',')return false;}}
        if(c=='['){v.kind=Json::Array;++p_;skip();if(p_<s_.size()&&s_[p_]==']'){++p_;return true;}for(;;){Json x;if(!value(x))return false;v.array.push_back(std::move(x));skip();if(p_>=s_.size())return false;char d=s_[p_++];if(d==']')return true;if(d!=',')return false;}}
        if(s_.compare(p_,4,"true")==0||s_.compare(p_,5,"false")==0||s_.compare(p_,4,"null")==0){if(s_[p_]=='t'){p_+=4;v.kind=Json::Number;v.number=1;}else if(s_[p_]=='f'){p_+=5;v.kind=Json::Number;v.number=0;}else{p_+=4;v.kind=Json::Null;}return true;}
        size_t start=p_;if(c=='-')++p_;while(p_<s_.size()&&((s_[p_]>='0'&&s_[p_]<='9')||s_[p_]=='.'||s_[p_]=='e'||s_[p_]=='E'||s_[p_]=='+'||s_[p_]=='-'))++p_;if(start==p_)return false;try{v.number=std::stod(s_.substr(start,p_-start));}catch(...){return false;}v.kind=Json::Number;return std::isfinite(v.number);
    }
    const std::string& s_;size_t p_=0;
};
const Json* get(const Json& j,const char* key){auto i=j.object.find(key);return i==j.object.end()?nullptr:&i->second;}
bool readVec(const Json* j,std::array<float,3>& v){if(!j||j->kind!=Json::Array||j->array.size()!=3)return false;for(int i=0;i<3;++i){if(j->array[i].kind!=Json::Number||!std::isfinite(j->array[i].number)||std::abs(j->array[i].number)>std::numeric_limits<float>::max())return false;v[i]=(float)j->array[i].number;}return true;}
}
SceneDocument::SceneDocument(){reset();}
void SceneDocument::reset(){objects_.clear();Object c;c.id=makeId();c.name=L"Camera";c.type=L"camera";objects_.push_back(c);Object m;m.id=makeId();m.name=L"Triangle Mesh";m.type=L"triangle";objects_.push_back(m);Object l;l.id=makeId();l.name=L"Directional Light";l.type=L"directional_light";objects_.push_back(l);filePath_.clear();dirty_=false;}
SceneDocument::Object* SceneDocument::find(const std::wstring& id) noexcept{for(auto& o:objects_)if(o.id==id)return &o;return nullptr;}
const SceneDocument::Object* SceneDocument::find(const std::wstring& id)const noexcept{for(const auto& o:objects_)if(o.id==id)return &o;return nullptr;}
std::wstring SceneDocument::addObject(const std::wstring& name,const std::wstring& type){Object o;o.id=makeId();o.name=name.empty()?L"Object":name;o.type=type;objects_.push_back(o);dirty_=true;return o.id;}
bool SceneDocument::removeObject(const std::wstring& id){auto i=std::find_if(objects_.begin(),objects_.end(),[&](const auto& o){return o.id==id;});if(i==objects_.end())return false;objects_.erase(i);dirty_=true;return true;}
bool SceneDocument::setObjectName(const std::wstring& id,const std::wstring& name){auto* o=find(id);if(!o||name.empty())return false;if(o->name!=name){o->name=name;dirty_=true;}return true;}
bool SceneDocument::setTransformComponent(const std::wstring& id,int component,double value){auto* o=find(id);if(!o||component<0||component>=9||!std::isfinite(value)||!std::isfinite((float)value))return false;auto& v=component<3?o->position:component<6?o->rotation:o->scale;int axis=component%3;float f=(float)value;if(v[axis]!=f){v[axis]=f;dirty_=true;}return true;}
bool SceneDocument::load(const std::wstring& path,std::wstring* error){
    std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f){if(error)*error=L"Cannot open scene file.";return false;}std::string bytes((std::istreambuf_iterator<char>(f)),{});Json root;if(!Parser(bytes).parse(root)){if(error)*error=L"Invalid JSON syntax.";return false;}
    const Json* format=get(root,"format"),*version=get(root,"version"),*objects=get(root,"objects");if(root.kind!=Json::Object||!format||format->kind!=Json::String||format->str!="FX3Scene"||!version||version->kind!=Json::Number||version->number!=1||!objects||objects->kind!=Json::Array){if(error)*error=L"Unsupported FX3Scene format/version.";return false;}
    std::vector<Object> loaded;std::unordered_set<std::wstring> ids;
    for(const auto& j:objects->array){const Json *id=get(j,"id"),*name=get(j,"name"),*type=get(j,"type");if(j.kind!=Json::Object||!id||!name||!type||id->kind!=Json::String||name->kind!=Json::String||type->kind!=Json::String){if(error)*error=L"Scene object has invalid identity fields.";return false;}Object o;o.id=wide(id->str);o.name=wide(name->str);o.type=wide(type->str);if(o.id.empty()||o.name.empty()||o.type.empty()||!ids.insert(o.id).second||!readVec(get(j,"position"),o.position)||!readVec(get(j,"rotation"),o.rotation)||!readVec(get(j,"scale"),o.scale)){if(error)*error=L"Scene object has missing, duplicate or invalid fields.";return false;}loaded.push_back(std::move(o));}
    objects_=std::move(loaded);filePath_=path;dirty_=false;return true;
}
bool SceneDocument::save(const std::wstring& path,std::wstring* error){
    if(path.empty()){if(error)*error=L"Scene path is empty.";return false;}auto temp=path+L".tmp";std::ofstream f(std::filesystem::path(temp),std::ios::binary|std::ios::trunc);if(!f){if(error)*error=L"Cannot create scene file.";return false;}
    f<<"{\n  \"format\": \"FX3Scene\",\n  \"version\": 1,\n  \"objects\": [\n";
    for(size_t i=0;i<objects_.size();++i){const auto& o=objects_[i];auto vec=[&](const auto& v){f<<"["<<std::setprecision(9)<<v[0]<<", "<<v[1]<<", "<<v[2]<<"]";};f<<"    { \"id\": "<<quote(o.id)<<", \"name\": "<<quote(o.name)<<", \"type\": "<<quote(o.type)<<", \"position\": ";vec(o.position);f<<", \"rotation\": ";vec(o.rotation);f<<", \"scale\": ";vec(o.scale);f<<" }"<<(i+1<objects_.size()?",":"")<<"\n";}
    f<<"  ]\n}\n";f.flush();if(!f){if(error)*error=L"Failed writing scene data.";f.close();DeleteFileW(temp.c_str());return false;}f.close();if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){if(error)*error=L"Could not atomically replace scene file.";DeleteFileW(temp.c_str());return false;}filePath_=path;dirty_=false;return true;
}
