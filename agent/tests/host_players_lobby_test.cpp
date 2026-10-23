#include "coop/net/peer_limits.h"
#include "../../src/votv-coop/src/coop/net/lobby_client.cpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {std::string body;}
namespace coop::net::http {
Response Request(const std::string&,const std::string&,const char*,const std::string&,int,size_t){return {true,200,test::body};}
}
namespace coop::text {
bool FromUtf8Strict(const char* p,size_t n,std::wstring* out){out->assign(p,p+n);return true;}
std::string ToUtf8(const std::wstring& s){return {s.begin(),s.end()};}
std::wstring FilterNameChars(const std::wstring& s){return s;}
}
namespace coop::game_build {bool KeyShapeOk(const std::string&){return false;}}
namespace coop::net::peer_identity {bool PublicKeyFromIdentityString(const std::string&,PubKey&){return false;}}
namespace ue_wrap::log {void Write(Level,const char*,...){} }
int main(){namespace L=coop::net::lobby;namespace J=coop::net::jsonu;
 auto fetch=[](const J::Json& max,const J::Json& current){J::Json row={{"lobbyId","one"},{"name","test"},{"players_max",max},{"players_cur",current}};
  test::body=J::Json{{"lobbies",J::Json::array({row})}}.dump();std::vector<L::LobbyRow> out;std::string status;
  CHECK(L::FetchList("test","",out,status)&&out.size()==1);return out[0];};
 for(int count=2;count<=32;++count){auto row=fetch(count,count);CHECK(row.playersMax==count && row.playersCur==count);}
 for(auto max:{J::Json(nullptr),J::Json("32")}){const auto row=fetch(max,999);CHECK(row.playersMax==4&&row.playersCur==4);}
 CHECK(fetch(32,-1).playersCur==1);CHECK(fetch(999,999).playersMax==32);
 CHECK(fetch(J::Json(std::numeric_limits<uint64_t>::max()),J::Json(std::numeric_limits<uint64_t>::max())).playersCur==32);
 CHECK(fetch(J::Json(std::numeric_limits<int64_t>::min()),0).playersMax==2);
 CHECK(J::IntClamped(J::Json{{"n",uint64_t{0}}},"n",-5,-1)==-1);
 std::puts("PASS real lobby list parser: all 31 counts, host inclusion, legacy defaults and signed/unsigned overflow bounds");
}
