#include <array>
#include <atomic>
#include <thread>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
constexpr int kMaxPeers=32;
struct Session {
 std::array<std::atomic<bool>,kMaxPeers> slotWorldReady_{};
 std::array<std::atomic<uint64_t>,kMaxPeers> slotWorldReadyRevision_{};
#include "session_world_ready.inc"
};
int main(){
 Session session;
 for(int slot=0;slot<kMaxPeers;++slot){
  CHECK(!session.IsSlotWorldReady(slot)&&session.SlotWorldReadyRevision(slot)==0);
  session.MarkSlotWorldReady(slot,true);const auto before=session.SlotWorldReadyRevision(slot);
  session.MarkSlotWorldReady(slot,false);session.MarkSlotWorldReady(slot,true);
  CHECK(session.IsSlotWorldReady(slot)&&session.SlotWorldReadyRevision(slot)==before+1);
  session.MarkSlotWorldReady(slot,true);CHECK(session.SlotWorldReadyRevision(slot)==before+1);
 }
 for(int slot:{-1,32,100}){session.MarkSlotWorldReady(slot,false);CHECK(!session.IsSlotWorldReady(slot)&&!session.SlotWorldReadyRevision(slot));}
 std::array<std::thread,8> threads;
 for(auto& thread:threads)thread=std::thread([&]{for(int i=0;i<128;++i)session.MarkSlotWorldReady(31,false);});
 for(auto& thread:threads)thread.join();
 CHECK(!session.IsSlotWorldReady(31)&&session.SlotWorldReadyRevision(31)==1025);
 CHECK(session.IsSlotWorldReady(1)&&session.SlotWorldReadyRevision(1)==1);
 puts("PASS UI06 actual Session world-ready revisions: resets between pumps, per-slot isolation, bounds and concurrent reset accounting");
}
