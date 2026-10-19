#include "coop/creatures/npc_class_policy.h"
#include <cassert>
#include <cwchar>
#include <cstdio>
struct Class { ue_wrap::reflection::FName name{}; Class* parent=nullptr; };
namespace ue_wrap::reflection {
bool IsDescendantOfAny(void*,void* const*,size_t,int){return false;} // No cached class loaded.
const FName& NameOf(void* p){return static_cast<Class*>(p)->name;}
void* SuperStructOf(void* p){return static_cast<Class*>(p)->parent;}
bool NameEquals(const FName& n,const wchar_t* s){
 return (n.ComparisonIndex==1&&!wcscmp(s,L"wisp_C"))||
 (n.ComparisonIndex==2&&!wcscmp(s,L"killerwisp_C"))||
 (n.ComparisonIndex==3&&!wcscmp(s,L"npc_zombie_C"));
}
}
int main(){const auto IsClassOrDerivedFromAnyAllowlisted=coop::npc_class_policy::MatchesAncestryName;
 Class wisp{{1,0}},killer{{2,0}},zombie{{3,0}},child{{4,0},&killer},other{{5,0}};
 assert(IsClassOrDerivedFromAnyAllowlisted(&wisp));assert(IsClassOrDerivedFromAnyAllowlisted(&killer));
 assert(IsClassOrDerivedFromAnyAllowlisted(&zombie));assert(IsClassOrDerivedFromAnyAllowlisted(&child));
 assert(!IsClassOrDerivedFromAnyAllowlisted(&other));assert(!IsClassOrDerivedFromAnyAllowlisted(nullptr));
 other.parent=&other;assert(!IsClassOrDerivedFromAnyAllowlisted(&other));
 puts("PASS first-spawn wisp/killer/other NPC/subclass matching without a loaded-class cache");
}
