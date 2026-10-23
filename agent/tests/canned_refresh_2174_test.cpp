#include "../../src/votv-coop/src/coop/props/prop_record_refresh.cpp"
#include "coop/player/hand_item.h"
#include "facts_test_support.h"
#include <cstdlib>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace {
struct WatchRow { std::wstring cls, name; int tag; ue_wrap::script_gate::PostFn post; };
std::vector<WatchRow> watches;
struct State { bool opened; int foods; } state{false, 5};
std::vector<State> published;
bool held = false, live = true, quiesced = true;
bool mutationReady = true;
std::wstring key = L"can1";
void Fire(const wchar_t* name, bool ours = false) {
    for (const auto& w : watches) if (w.cls == L"prop_cannedFood_C" && w.name == name) {
        ue_wrap::script_gate::Call c{};
        c.object = &state; c.tag = w.tag; c.fromOurCode = ours;
        w.post(c); return;
    }
    CHECK(false); // the old implementation never watched either can mutation
}
}
namespace ue_wrap::game_thread { bool IsGameThread() { return true; } }
namespace ue_wrap::script_gate {
bool Watch(const facts::Resolved&,int,PreFn,PostFn) { return true; }
bool Unwatch(const facts::Resolved&,int,PreFn,PostFn) { return true; }
bool WatchClassName(const wchar_t* c,const wchar_t* n,int tag,PreFn,PostFn post) {
    watches.push_back({c,n,tag,post}); return true;
}
bool Watch(void*,int,PreFn,PostFn) { return true; }
bool Unwatch(void*,int,PreFn,PostFn) { return true; }
}
namespace ue_wrap::facts { Resolved Resolve(const Function&) { return {}; } }
namespace ue_wrap::reflection {
void* FindClass(const wchar_t*) { return nullptr; }
void* FindDispatchFunctionCached(void*,const wchar_t*) { std::abort(); }
void* ClassOf(void*) { std::abort(); }
int32_t FindPropertyOffset(void*,const wchar_t*) { std::abort(); }
bool IsLive(void*) { return live; }
}
namespace ue_wrap::prop {
std::wstring GetInteractableKeyString(void*) { return key; }
std::wstring GetPropNameString(void*) { std::abort(); }
}
namespace ue_wrap::fname_utils {
reflection::FName StringToFName(const std::wstring&) { std::abort(); }
}
namespace ue_wrap {
ParamFrame::ParamFrame(void*) { std::abort(); }
bool Call(void*,ParamFrame&) { std::abort(); }
}
namespace coop::hand_item { bool IsHandAxisActor(void*) { return held; } }
namespace coop::join_membership_sweep { bool HasLoadTailQuiesced() { return quiesced; } }
namespace coop::prop_save_data {
bool PrepareForMutation(void*,const std::wstring&) { return mutationReady; }
bool Publish(coop::net::Session*,void*,const std::wstring& k) {
    CHECK(k == key); published.push_back(state); return true;
}
}
int main() {
    namespace S = coop::props::prop_record_refresh;
    coop::net::Session session;
    S::Install(&session);
    state.opened = true; Fire(L"open");
    CHECK(published.size() == 1 && published.back().opened);
    // Opening and successive bites inside the old 300ms coalescing window all matter.
    --state.foods; Fire(L"eat");
    --state.foods; Fire(L"eat");
    CHECK(published.size() == 3 && published.back().foods == 3);
    session.mode = coop::net::Role::Client;
    --state.foods; Fire(L"eat");
    CHECK(published.size() == 4 && published.back().foods == 2);
    Fire(L"eat", true); // replay must not feed back onto the wire
    held = true; Fire(L"eat"); held = false; // hand records use another lifecycle
    live = false; Fire(L"open"); live = true;
    quiesced = false; Fire(L"open"); quiesced = true;
    key = L"None"; Fire(L"open"); key = L"can1";
    session.linked = false; Fire(L"eat"); session.linked = true;
    CHECK(published.size() == 4);
    S::OnDisconnect(); Fire(L"eat"); CHECK(published.size() == 4);
    S::Install(&session); CHECK(watches.size() == 2);
    Fire(L"eat"); CHECK(published.size() == 5);
    ue_wrap::script_gate::Call c{};c.object=&state;
    CHECK(S::OnCanChanging(c)==ue_wrap::script_gate::Verdict::Run);
    mutationReady=false;
    CHECK(S::OnCanChanging(c)==ue_wrap::script_gate::Verdict::Cancel);
    mutationReady=true;quiesced=false;
    CHECK(S::OnCanChanging(c)==ue_wrap::script_gate::Verdict::Cancel);
    held=true;CHECK(S::OnCanChanging(c)==ue_wrap::script_gate::Verdict::Run);
    std::puts("PASS canned-food world open/eat publication, rapid changes and replay/lifecycle guards");
}
