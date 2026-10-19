#include "../../src/votv-coop/src/ue_wrap/core/script_gate.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
// Engine boundary doubles; no installation/VM dispatch is exercised by this scope test.
namespace ue_wrap::reflection {
bool InCoopDispatch(){return false;} const FName& NameOf(void*){static FName n{};return n;}
void* OuterOf(void*){return nullptr;} void* FindFunction(void*,const wchar_t*){return nullptr;}
std::wstring ToString(const FName&){return L"";}
}
namespace ue_wrap::fname_utils {reflection::FName StringToFName(const std::wstring&){return {};}}
namespace ue_wrap::game_thread {void Post(std::function<void()>){std::abort();}bool IsGameThread(){return true;}}
namespace ue_wrap::hook {bool Init(){std::abort();}bool Install(void*,void*,void**,bool){std::abort();}bool VerifyEntries(const char*){return true;}}
namespace ue_wrap::log {void Write(Level,const char*,...) {}}
namespace ue_wrap::object_index {uint64_t ClassSetVersion(){return 0;}void* ClassByName(const wchar_t*){return nullptr;}}
namespace ue_wrap::script_loop {
std::uintptr_t* ResolveGNatives(){return nullptr;}bool TableFilled(const std::uintptr_t*){return false;}
std::uintptr_t ByCode(std::uintptr_t,int&){return 0;}std::uintptr_t ByHandlers(const std::uintptr_t*,std::uintptr_t&,std::uintptr_t&){return 0;}
}
namespace ue_wrap {void MainModuleRange(std::uintptr_t& a,std::uintptr_t& b){a=b=0;}}
int main() {
 using namespace ue_wrap::script_gate;
 int fn=0,innerFn=0,actor=0,other=0;
 CHECK(!IsObjectBodyActive(&fn,&actor));
 try {
  ActiveScope outer(1,&actor,&fn,nullptr,nullptr);
  CHECK(IsObjectBodyActive(&fn,&actor)&&!IsObjectBodyActive(&fn,&other));
  {ActiveScope inner(2,&other,&innerFn,nullptr,&fn);
   CHECK(IsObjectBodyActive(&fn,&actor)&&IsObjectBodyActive(&innerFn,&other));
   CHECK(IsBodyActive(&innerFn,&fn)&&!IsBodyActive(&innerFn,&innerFn));}
  CHECK(IsObjectBodyActive(&fn,&actor)&&!IsObjectBodyActive(&innerFn,&other));
  throw 1;
 } catch(int) {}
 CHECK(!IsObjectBodyActive(&fn,&actor)&&!IsBodyActive(&fn)&&!CurrentThreadCall().active);
 CHECK(!IsObjectBodyActive(nullptr,&actor)&&!IsObjectBodyActive(&fn,nullptr));
 puts("PASS real script-gate body scopes: exact receiver, nested bodies, caller match and exception cleanup");
}
