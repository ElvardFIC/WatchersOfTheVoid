// Exact production Covers/claim functions, with engine class/name boundaries doubled.
#include "support_2135.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include <map>
#include <set>
#include <cwchar>
#include <cstring>
#include "ue_wrap/core/object_index.h"
namespace test {
inline bool overrides = true, cycle = false;
inline int names = 0;
inline std::wstring leaf = L"prop_leaf_C", parent = L"prop_family_C";
inline bool classLoaded=false;inline unsigned classLookups=0,customLookups=0;
}
namespace ue_wrap::object_index {
void* ClassByName(const wchar_t*){++test::classLookups;return test::classLoaded?&test::actor:nullptr;}
}
namespace ue_wrap::save_record {
using ClassResolver=void* (*)(const wchar_t*);
#include "../../_research/tests_2157/class_field.h"
}
namespace ue_wrap::reflection {
const FName& NameOf(void* p) { static FName n; n.ComparisonIndex = p == &test::actor ? 1 : 2; return n; }
std::wstring ToString(const FName& n) { ++test::names; return n.ComparisonIndex == 1 ? test::leaf : test::parent; }
void* SuperStructOf(void* p) { return p == &test::actor ? &test::ui : test::cycle ? &test::actor : nullptr; }
void* FindClass(const wchar_t*) { std::abort(); } // a whole-array lookup is forbidden here
}
namespace coop::prop_save_data {
namespace R = ue_wrap::reflection;
namespace SR { bool OverridesGetData(void*) { return test::overrides; } }
std::set<std::wstring, std::less<>> g_ownedElsewhere;
struct OwnedClass { ue_wrap::CachedObjRef identity; bool owned = false; };
std::map<void*, OwnedClass> g_ownedCache;
#include "../../_research/tests_2157/coverage.h"
}
int main() {
 using namespace test; namespace S = coop::prop_save_data;
 void* restored=&ui;
 for(int i=0;i<4096;++i)CHECK(!ue_wrap::save_record::WriteClassField(&restored,L"not_loaded_C",nullptr) && !restored);
 CHECK(classLookups==4096);
 CHECK(ue_wrap::save_record::WriteClassField(&restored,L"",nullptr) && !restored && classLookups==4096);
 classLoaded=true;
 CHECK(ue_wrap::save_record::WriteClassField(&restored,L"not_loaded_C",nullptr) && restored==&actor);
 CHECK(ue_wrap::save_record::WriteClassField(&restored,L"custom",[](const wchar_t*)->void*{++customLookups;return &ui;}) && restored==&ui && customLookups==1);
 puts("PASS stow record classes: 4096 indexed misses without array scans, null names, late readiness and explicit resolver");
 CHECK(S::Covers(&actor)); CHECK(names == 0);
 S::DeclareClassOwnedElsewhere(L"not_loaded_C");
 CHECK(S::Covers(&actor)); CHECK(names == 2);
 CHECK(S::Covers(&actor)); CHECK(names == 2); // warm class does no name work
 S::DeclareClassOwnedElsewhere(L"not_loaded_C");
 CHECK(S::Covers(&actor)); CHECK(names == 2); // repeated Install keeps cache
 S::DeclareClassOwnedElsewhere(L"PROP_FAMILY_c");
 CHECK(!S::Covers(&actor)); CHECK(names == 4); // inherited, case-insensitive ownership
 S::g_ownedElsewhere.clear(); S::g_ownedCache.clear();
 S::DeclareClassOwnedElsewhere(L"prop_leaf_C"); CHECK(!S::Covers(&actor));
 leaf = L"replacement_C"; ++actor.serial; // recycled UClass address cannot reuse exclusion
 CHECK(S::Covers(&actor));
 overrides = false; CHECK(!S::Covers(&actor)); overrides = true;
 S::g_ownedCache.clear(); cycle = true; names = 0;
 CHECK(!S::Covers(&actor)); CHECK(names == 64); CHECK(S::g_ownedCache.empty());
 cycle = false; ui.live = false; CHECK(!S::Covers(&actor)); ui.live = true;
 CHECK(S::Covers(&actor)); CHECK(!S::Covers(nullptr));
 puts("PASS stow coverage: no object-array walks, ancestry/case, cache/redeclaration, recycling, bounded invalid chains");
}
