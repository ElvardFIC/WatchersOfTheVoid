#include "../../src/votv-coop/src/coop/interactables/shared_screen_sync.cpp"
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include <cassert>
#include <cstdio>
namespace test {
uint8_t holder=1;
bool local=false, widgetReady=true;
int applies=0, tab=-1;
std::vector<ue_wrap::shared_screen::Row> source;
std::vector<uint8_t> State(int tab){
    ue_wrap::shared_screen::Row row;row.path="pc/r/0";row.flags=1;row.tab=tab;
    return coop::shared_screen_wire::Encode({row});
}
coop::net::BlobChunkPayload Chunk(const std::vector<uint8_t>& bytes,uint32_t sequence){
    coop::net::BlobChunkPayload chunk{};chunk.blobSeq=sequence;chunk.chunks=1;
    assert(bytes.size()<=sizeof(chunk.data));chunk.chunkLen=static_cast<uint16_t>(bytes.size());
    std::memcpy(chunk.data,bytes.data(),bytes.size());return chunk;
}
void Tick(){coop::shared_screen_sync::g_next={};coop::shared_screen_sync::Tick();}
int SentTab(const coop::net::BlobChunkPayload& chunk){
    std::vector<ue_wrap::shared_screen::Row> rows;
    assert(coop::shared_screen_wire::Decode({chunk.data,chunk.data+chunk.chunkLen},rows));return rows[0].tab;
}
}
namespace coop::device_occupancy {
uint8_t HolderOf(const wchar_t*){return test::holder;}
bool LocalHolds(const wchar_t*){return test::local;}
}
namespace ue_wrap::shared_screen {
bool Read(std::vector<Row>& rows){rows=test::source;return test::widgetReady;}
bool Apply(const std::vector<Row>& rows){++test::applies;if(!test::widgetReady)return false;test::tab=rows[0].tab;return true;}
}
namespace ue_wrap::log {void Write(Level,const char*,...){} }
int main(){
    using namespace coop;using namespace test;
    net::Session session;session.ready[1]=session.ready[2]=true;
    shared_screen_sync::Install(&session);
    shared_screen_sync::OnChunk(Chunk(State(3),1),2); // Another peer cannot author the operator's UI.
    Tick();assert(applies==0&&session.sent.empty());
    session.refuse=true;
    shared_screen_sync::OnChunk(Chunk(State(3),2),1);
    Tick();assert(tab==3&&session.attempts==1&&session.sent.empty());
    shared_screen_sync::OnChunk(Chunk(State(4),3),1);
    session.refuse=false;Tick();
    assert(tab==4&&session.sent.size()==1&&session.sent[0].first==2&&SentTab(session.sent[0].second)==4);
    shared_screen_sync::QueueConnectBroadcastForSlot(3);
    Tick();assert(session.sent.size()==1);
    session.ready[3]=true;Tick();
    assert(session.sent.back().first==3&&SentTab(session.sent.back().second)==4);
    holder=2;Tick();
    shared_screen_sync::OnChunk(Chunk(State(7),4),1);Tick();assert(tab==4);
    shared_screen_sync::OnDisconnectForSlot(2);
    shared_screen_sync::OnDisconnect();
    session.mode=net::Role::Client;session.sent.clear();holder=1;local=false;
    shared_screen_sync::OnChunk(Chunk(State(5),5),2);Tick();assert(tab==4);
    widgetReady=false;shared_screen_sync::OnChunk(Chunk(State(6),6),0);Tick();assert(tab==4);
    widgetReady=true;Tick();assert(tab==6); // Delayed widget creation retries the host snapshot.
    local=true;source={};ue_wrap::shared_screen::Row row;row.path="pc/r/0";row.flags=1;row.tab=9;source.push_back(row);
    Tick();assert(session.sent.back().first==0&&SentTab(session.sent.back().second)==9);
    local=false;Tick();assert(tab==9); // Releasing preserves the new operator's final screen.
    shared_screen_sync::OnDisconnect();
    const int before=applies;Tick();assert(applies==before);
    puts("PASS shared screen operator trust, latest-state retry, join readiness, handoff, widget retry and teardown");
}
