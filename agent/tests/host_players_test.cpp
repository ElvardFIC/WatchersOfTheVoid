#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include "coop/config/config.h"
#include "ue_wrap/core/log.h"
#include "ui/native_screen.h"
#ifndef HOST_PLAYERS_SOURCE
#define HOST_PLAYERS_SOURCE "../../src/votv-coop/src/ui/host_player_limit.cpp"
#endif
#include HOST_PLAYERS_SOURCE
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
struct Widget {bool hovered=false,alive=true;float opacity=1;std::wstring text;};
std::vector<std::unique_ptr<Widget>> widgets;
int persisted=4,reads=0,writes=0,allocations=0,failAt=0;
coop::config::SetResult writeResult=coop::config::SetResult::Saved;
Widget* Node(void* p){auto* w=static_cast<Widget*>(p);CHECK(w&&w->alive);return w;}
void* Make(const wchar_t* text){if(++allocations==failAt)return nullptr;auto w=std::make_unique<Widget>();w->text=text;auto* out=w.get();widgets.push_back(std::move(w));return out;}
void Reset(int saved=4){ui::host_player_limit::ClearWidgets();ui::host_player_limit::g_players=0;ui::host_player_limit::g_savePending=false;
 widgets.clear();persisted=saved;reads=writes=allocations=failAt=0;writeResult=coop::config::SetResult::Saved;}
}
namespace coop::config {
long ResolveInt(const config_registry::IntRow& row){CHECK(row.row==config_registry::rows::net_player_limit.row);++test::reads;return test::persisted;}
SetResult SetValue(const config_registry::IntRow& row,const char* value){
 CHECK(row.row==config_registry::rows::net_player_limit.row);++test::writes;
 if(test::writeResult==SetResult::Saved)test::persisted=std::stoi(value);return test::writeResult;
}
}
namespace ue_wrap::engine {
bool SetWidgetText(void* p,const wchar_t* text){test::Node(p)->text=text;return true;}
bool WidgetIsHovered(void* p){return test::Node(p)->hovered;}
bool SetWidgetRenderOpacity(void* p,float value){test::Node(p)->opacity=value;return true;}
}
namespace ui::native_screen {
FLinearColor Text(){return {};}
void* BuildButton(void*,void*,const wchar_t* text,int32_t){return test::Make(text);}
void* AddText(void*,const wchar_t* text,int32_t,const FLinearColor&,uint8_t,float){return test::Make(text);}
void* SlotOf(void* p){return p;}
void SetHSlot(void*,float,uint8_t,uint8_t){}
}
namespace ue_wrap::log {void Write(Level,const char*,...){} std::string Addr(std::string_view value){return std::string(value);} }
int main(){namespace H=ui::host_player_limit;using namespace test;
 Reset(17);CHECK(H::Value()==17 && H::Value()==17 && reads==1);
 CHECK(H::Build(nullptr,nullptr));CHECK(Node(H::g_label)->text==L"17");
 H::Select(2);H::Refresh();CHECK(Node(H::g_less)->opacity<1 && persisted==2);
 Node(H::g_less)->hovered=true;const auto before=writes;CHECK(H::HandleClick()&&H::Value()==2&&writes==before);Node(H::g_less)->hovered=false;
 Node(H::g_more)->hovered=true;
 for(int count=3;count<=32;++count){CHECK(H::HandleClick());CHECK(H::Value()==count && persisted==count);CHECK(Node(H::g_label)->text==std::to_wstring(count));}
 CHECK(Node(H::g_more)->opacity<1);CHECK(H::HandleClick()&&H::Value()==32);Node(H::g_more)->hovered=false;
 for(size_t i=0;i<H::kPresets.size();++i){Node(H::g_presets[i])->hovered=true;CHECK(H::HandleClick());CHECK(H::Value()==H::kPresets[i]);Node(H::g_presets[i])->hovered=false;}
 // Render-thread changes touch the shared selection, never game-thread widgets or disk per frame.
 H::Select(7);H::Refresh();const auto oldWrites=writes;const auto oldLabel=Node(H::g_label)->text;
 for(int count=2;count<=32;++count)H::Select(count,false);
 CHECK(writes==oldWrites && H::Value()==32 && Node(H::g_label)->text==oldLabel);
 H::Remember();CHECK(writes==oldWrites+1 && persisted==32);H::Refresh();
 H::ClearWidgets();for(auto& w:widgets)w->alive=false;CHECK(!H::HandleClick());
 H::g_players=0;CHECK(H::Value()==32);CHECK(H::Build(nullptr,nullptr));CHECK(Node(H::g_label)->text==L"32");
 H::Select(-100);CHECK(H::Value()==2);H::Select(100);CHECK(H::Value()==32);
 for(int point=1;point<=8;++point){Reset();failAt=point;CHECK(!H::Build(nullptr,nullptr));CHECK(!H::HandleClick()&&!H::g_label&&!H::g_less&&!H::g_more);}
 Reset();writeResult=coop::config::SetResult::HeldNotSaved;H::Select(19);CHECK(H::Value()==19&&H::g_savePending);
 writeResult=coop::config::SetResult::Saved;H::Remember();CHECK(persisted==19&&!H::g_savePending);
 const auto* row=coop::config_registry::rows::net_player_limit.row;
 CHECK(std::string(row->key)=="net.player_limit" && row->defI==4);
 const auto writeCount=writes;
 auto choose=[] {for(int i=0;i<1000;++i)H::Select(2+i%31,false);};
 std::thread a(choose),b(choose);a.join();b.join();
 CHECK(writes==writeCount);H::Remember();CHECK(persisted==H::Value());
 std::puts("PASS actual host selector: all 31 limits, presets, bounds, shared overlay/native choice, deferred persistence, reopen and failed widget construction");
}
