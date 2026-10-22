"""Compile production class resolvers and exercise loading and worker-thread boundaries."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build/upstream-check"
source = (ROOT / "src/votv-coop/src/ue_wrap/actors/prop.cpp").read_text()
base = source[source.index("void SayAskedOffThread("):source.index("// Raw reads at an offset")]
extras = source[source.index("// Per-class sticky resolution:"):source.index("// Declared in prop.h")]
extras = extras[:extras.rindex("}  // namespace")]
fixture = r'''
#include <atomic>
#include <string>
#include <unordered_map>
#include <cassert>
#include <cstdio>
struct Cls {Cls* parent=nullptr;};
Cls prop,clump,chip,trash,child;
bool worker=false;int lookups=0;
std::unordered_map<std::wstring,void*> classes;
namespace game_thread {bool IsDefinitelyOffGameThread(){return worker;}}
namespace P::name {const wchar_t* PropClass=L"prop_C";}
namespace object_index {
void* ClassByName(const wchar_t* name){assert(!worker);++lookups;return classes[name];}
}
namespace R {void* FindClass(const wchar_t*){assert(false);return nullptr;}}
namespace ue_wrap {struct CachedObjRef {
void* p=nullptr;bool Alive()const{return p!=nullptr;}void* Raw()const{return p;}void Set(void* v){p=v;}
};}
#define UE_LOGE(...) ((void)0)
bool WalksToBase(void* cls,void* base){if(!base)return false;for(int i=0;cls&&i<16;++i){if(cls==base)return true;cls=static_cast<Cls*>(cls)->parent;}return false;}
@BASE@
@EXTRAS@
int main(){
 worker=true;assert(!PropBaseClass());ResolveExtraBases();assert(lookups==0);
 worker=false;assert(!PropBaseClass());classes[L"prop_C"]=&prop;assert(PropBaseClass()==&prop);
 int before=lookups;worker=true;assert(PropBaseClass()==&prop);assert(lookups==before);worker=false;
 child.parent=&chip;classes[L"late_C"]=&child;
 auto first=ResolveTrashClassKind(L"late_C",object_index::ClassByName);
 assert(!first.isTrash&&g_trashClassKind.empty());
 // The base finishes loading without any change in the set of class names.
 classes[L"actorChipPile_C"]=&chip;
 assert(ResolveTrashClassKind(L"late_C",object_index::ClassByName).isTrash);
 assert(g_trashClassKind.empty()); // unresolved clump base can still change isClump
 classes[L"prop_garbageClump_C"]=&clump;classes[L"trashBitsPile_C"]=&trash;
 auto ready=ResolveTrashClassKind(L"late_C",object_index::ClassByName);
 assert(ready.isTrash&&!ready.isClump&&g_trashClassKind.size()==1);
 before=lookups;assert(ResolveTrashClassKind(L"late_C",object_index::ClassByName).isTrash);assert(lookups==before);
 worker=true;assert(!ResolveTrashClassKind(L"late_C",object_index::ClassByName).isTrash);assert(lookups==before);
 worker=false;assert(!ResolveTrashClassKind(L"unknown_C",object_index::ClassByName).isTrash);
 classes[L"unknown_C"]=&clump;assert(ResolveTrashClassKind(L"unknown_C",object_index::ClassByName).isClump);
 puts("PASS production prop classes: late bases, definitive cache, unknown names and worker isolation");
}
'''.replace("@BASE@", base).replace("@EXTRAS@", extras)
OUT.mkdir(parents=True, exist_ok=True)
cpp = OUT / "prop_classes.cpp"
cpp.write_text(fixture)
exe = OUT / "prop_classes.exe"
subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8", f"/Fo{OUT / 'prop_classes.obj'}", f"/Fe{exe}", str(cpp)], check=True)
subprocess.run([str(exe)], check=True)
