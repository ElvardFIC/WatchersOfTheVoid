"""Compile the actual container seed, publication and retry functions with a refused transport."""
from pathlib import Path
import subprocess

CHECKOUT = Path(__file__).resolve().parents[2]
SOURCE = CHECKOUT / "src/votv-coop/src/coop/props/container_contents_sync.cpp"
OUTPUT = CHECKOUT / "build/sync-2157-check"


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


FIXTURE = r'''
#include <cstdint>
#include <atomic>
#include <array>
#include "coop/props/container_seed_retry.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
#define UE_LOGI(...) ((void)0)
#define UE_LOGW(...) ((void)0)
int actor, targeted=0, fanouts=0, accepted=0;
bool live=true, inventoryReady=true, readable=true, classReady=true;
namespace coop::net {struct Session{bool connected()const{return true;}bool send=true;int blocked=-1;uint32_t generation=1;bool HasPeerConn(int)const{return true;}bool IsSlotWorldReady(int)const{return true;}uint32_t peerGenerationForSlot(int)const{return generation;}};inline constexpr int kMaxPeers=32;enum class ReliableKind{ContainerContents};}
namespace coop::element {
enum class ElementType{Prop};
struct Registry {
 struct ActorIdPair{uint32_t id;void* actor;int internalIdx;};
 static Registry& Get(){static Registry r;return r;}
 void SnapshotActorsByType(ElementType,std::vector<ActorIdPair>& out){out.push_back({7,&actor,1});}
};
}
namespace SR {struct SaveRecord{};}
namespace R {bool IsLiveByIndex(void*,int){return live;}void* ClassOf(void* p){return p;}}
namespace ci {
bool IsContainer(void*){return true;}void* InventoryOf(void* p){return inventoryReady?p:nullptr;}
bool IsWorldInventory(void*){return true;}void* OwnerOf(void* p){return p;}
void RederiveShownState(void*,void*){}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return 1;}}
namespace ue_wrap::prop {bool WalksToBase(void*,void*){return true;}}
namespace cb {
enum class Owed{None,Pending,GivenUp};
bool IsBirthSlice(uint32_t){return false;}void FirstSent(uint32_t){}void Forget(uint32_t){}
Owed FirstOwed(uint32_t,uint64_t){return Owed::None;}bool Awaited(void*,int){return false;}
}
namespace cw {
constexpr uint64_t kBirthBase=~uint64_t{0};
std::vector<uint8_t> Pack(uint32_t,uint64_t,const std::vector<SR::SaveRecord>&){return {1,2,3};}
uint64_t ContentHash(uint32_t,const std::vector<SR::SaveRecord>&){return 99;}
}
namespace wp {void NotePublished(uint32_t,uint64_t){}}
namespace coop::blob_chunks {
size_t MaxBlobBytes(){return 1000;}
bool SendBlob(net::Session* s,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){if(s->send)++fanouts;return s->send;}
bool SendBlobToSlot(net::Session* s,int slot,net::ReliableKind,uint32_t,const std::vector<uint8_t>&){++targeted;const bool ok=s->send&&s->blocked!=slot;if(ok)++accepted;return ok;}
}
std::atomic<coop::net::Session*> g_session;
std::set<uint32_t> g_dirty,g_retry;
coop::props::ContainerSeedRetries g_seedRetry;
std::array<uint32_t,32> g_seedScans{};
void QueueConnectBroadcastForSlot(int);
std::map<uint32_t,uint64_t> g_sentHash,g_baseHash;
uint32_t g_nextSeq=1;
constexpr uint64_t kUnsentWarnMs=5000;
bool IsHost(){return true;}uint64_t NowMs(){return 1000;}
void* ContainerClass(){return classReady?&actor:nullptr;}void* InventoryOf(void* p){return ci::InventoryOf(p);}
bool IsWorldContainerInventory(void*){return true;}
void* LivePropActor(uint32_t){return live?&actor:nullptr;}
enum class SliceRead{Ok,Unreadable,OverCap};
SliceRead ReadContents(void*,std::vector<SR::SaveRecord>& out){out.push_back({});return readable?SliceRead::Ok:SliceRead::Unreadable;}
@FUNCTIONS@
int main(){
 coop::net::Session s;g_session=&s;g_sentHash[7]=99;
 classReady=false;QueueConnectBroadcastForSlot(2);CHECK(g_seedScans[2]);classReady=true;RetrySeeds(&s);CHECK(!g_seedScans[2]&&accepted==1);targeted=accepted=0;
 s.send=false;QueueConnectBroadcastForSlot(2);CHECK(targeted==1&&g_seedRetry.Size()==1);
 s.send=true;RetrySeeds(&s);CHECK(targeted==2&&accepted==1&&g_seedRetry.Size()==0&&fanouts==0);
 g_dirty.insert(7);DrainDirty(&s);CHECK(fanouts==0);
 readable=false;QueueConnectBroadcastForSlot(3);CHECK(g_seedRetry.Size()==1);
 RetrySeeds(&s);CHECK(g_seedRetry.Size()==1&&accepted==1);
 readable=true;RetrySeeds(&s);CHECK(accepted==2&&g_seedRetry.Size()==0);
 inventoryReady=false;QueueConnectBroadcastForSlot(3);RetrySeeds(&s);CHECK(g_seedRetry.Size()==1);
 inventoryReady=true;RetrySeeds(&s);CHECK(accepted==3&&g_seedRetry.Size()==0);
 s.send=false;QueueConnectBroadcastForSlot(2);QueueConnectBroadcastForSlot(3);CHECK(g_seedRetry.Size()==2);
 s.send=true;s.blocked=2;RetrySeeds(&s);CHECK(accepted==4&&g_seedRetry.Size()==1);
 s.blocked=-1;RetrySeeds(&s);CHECK(accepted==5&&g_seedRetry.Size()==0);
 s.send=false;QueueConnectBroadcastForSlot(3);++s.generation;RetrySeeds(&s);CHECK(g_seedRetry.Size()==0);
 inventoryReady=false;QueueConnectBroadcastForSlot(3);live=false;RetrySeeds(&s);CHECK(g_seedRetry.Size()==0);
 coop::props::ContainerSeedRetries ledger;ledger.Queue(7,31,1);
 ledger.Drain([&](uint32_t,int,uint32_t){ledger.Queue(7,31,2);return true;});CHECK(ledger.Size()==1);
 ledger.Drain([&](uint32_t,int,uint32_t){ledger.Clear();ledger.Queue(8,31,2);return true;});CHECK(ledger.Size()==1);
 ledger.Drain([](uint32_t eid,int slot,uint32_t generation){CHECK(eid==8&&slot==31&&generation==2);return true;});CHECK(ledger.Size()==0);
 puts("PASS actual container seeds: exact recipient, refusal, prior hash, readiness, seat reuse, destruction and reentry");
}
'''


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = SOURCE.read_text(encoding="utf-8-sig")
    bodies = "\n".join(function(source, name) for name in (
        "bool BroadcastContainer(", "void DrainDirty(", "void RetrySeeds(", "void QueueConnectBroadcastForSlot("))
    fixture = OUTPUT / "container-seed.cpp"
    fixture.write_text(FIXTURE.replace("@FUNCTIONS@", bodies), encoding="utf-8")
    exe = OUTPUT / "container-seed.exe"
    subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8",
                    f"/I{CHECKOUT / 'src/votv-coop/include'}", f"/Fo{OUTPUT / 'container-seed.obj'}", f"/Fe{exe}", str(fixture)], check=True)
    subprocess.run([str(exe)], check=True)
    # A failed targeted seed must not be hidden by an older fan-out's identical hash.
    legacy = bodies.replace("g_seedRetry.Queue(static_cast<uint32_t>(pr.id), peerSlot, s->peerGenerationForSlot(peerSlot));", "g_retry.insert(static_cast<uint32_t>(pr.id));")
    control = OUTPUT / "container-seed-legacy.cpp"
    control.write_text(FIXTURE.replace("@FUNCTIONS@", legacy), encoding="utf-8")
    control_exe = OUTPUT / "container-seed-legacy.exe"
    subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8",
                    f"/I{CHECKOUT / 'src/votv-coop/include'}", f"/Fo{OUTPUT / 'container-seed-legacy.obj'}", f"/Fe{control_exe}", str(control)], check=True)
    result = subprocess.run([str(control_exe)], capture_output=True, text=True)
    assert result.returncode == 1 and "g_seedRetry.Size()==1" in result.stderr, result
    print("PASS negative control: previous seed retry silently loses the initial contents")


if __name__ == "__main__":
    main()
