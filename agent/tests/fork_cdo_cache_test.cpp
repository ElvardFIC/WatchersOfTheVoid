#include <cstdint>
#include <cwchar>
#include <mutex>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace fixture {
struct Obj {std::wstring name;Obj* cls=nullptr;int serial=1;bool live=true;};
Obj clsA{L"A"},clsB{L"B"},a{L"Default__A",&clsA},b{L"Default__B",&clsB};
int walks=0;
}
namespace ue_wrap {
struct CachedObjRef {
    fixture::Obj* object=nullptr;int serial=0;
    void Set(void* p){object=static_cast<fixture::Obj*>(p);serial=object?object->serial:0;}
    void* Raw()const{return object;}
    void* Get()const{return object&&object->live&&object->serial==serial?object:nullptr;}
};
}
namespace ue_wrap::reflection {
const std::wstring& NameOf(void* p){CHECK(p);return static_cast<fixture::Obj*>(p)->name;}
void* ClassOf(void* p){return static_cast<fixture::Obj*>(p)->cls;}
bool NameEquals(const std::wstring& name,const wchar_t* want){return name==want;}
void* FindObject(const wchar_t* name){++fixture::walks;for(auto* p:{&fixture::a,&fixture::b})if(p->live&&p->name==name)return p;return nullptr;}
#include "fork_cdo_cache.inc"
}
int main() {
    using namespace ue_wrap::reflection;
    using namespace fixture;
    CHECK(!FindClassDefaultObject(nullptr)&&walks==0);
    CHECK(FindClassDefaultObject(L"A")==&a&&walks==1);
    for(int i=0;i<10000;++i)CHECK(FindClassDefaultObject(L"A")==&a);
    CHECK(walks==1);
    ++a.serial;CHECK(FindClassDefaultObject(L"A")==&a&&walks==2);
    a.name=L"Default__retired";CHECK(!FindClassDefaultObject(L"A")&&walks==3);
    a.name=L"Default__A";CHECK(FindClassDefaultObject(L"A")==&a&&walks==4);
    ue_wrap::CachedObjRef other;other.Set(&b);
    g_cdoCache[Fnv1a(L"A")]={L"B",other};
    CHECK(FindClassDefaultObject(L"A")==&a&&walks==5);
    CHECK(!FindClassDefaultObject(L"Missing")&&!FindClassDefaultObject(L"Missing")&&walks==7);
    CHECK(FindClassDefaultObject(L"B")==&b&&walks==8);
    b.live=false;CHECK(!FindClassDefaultObject(L"B")&&walks==9);
    std::puts("PASS production CDO cache: 10000 hits, serial/name retirement, hash-owner mismatch and retryable misses");
}
