"""Exercise production recipient selection and held-food addresses at the final seat."""
from pathlib import Path
import subprocess

from container_seed_retry_test import function

CHECKOUT = Path(__file__).resolve().parents[2]
ROOT = CHECKOUT / "src/votv-coop/src/coop"
OUT = CHECKOUT / "build/sync-2157-check"

PREFIX = r'''
#include "coop/net/peer_limits.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace coop::net {
inline constexpr int kMaxPeers=kPeerCapacity;
enum class Role{Host,Client};
struct Session {
 Role mode=Role::Host;
 Role role()const{return mode;}
 bool IsSlotReady(int slot)const{return slot>0&&slot<kMaxPeers;}
 bool IsSlotWorldReady(int slot)const{return IsSlotReady(slot);}
};
}
namespace net=coop::net;
net::Session session;
net::Session* g_session=&session;
net::Session* Session(){return g_session;}
'''


def main():
    cases = (
        ("ext", "player/hand_extinguisher_sync.cpp", "uint32_t ReadyMask("),
        ("fire", "player/hand_fire_sync.cpp", "uint32_t Destinations("),
        ("grill", "interactables/grill_hand_sync.cpp", "uint32_t Mask("),
        ("blast", "props/explosion_events.cpp", "uint32_t Destinations("),
    )
    bodies = []
    for scope, file, signature in cases:
        source = (ROOT / file).read_text(encoding="utf-8-sig")
        bodies.append(f"namespace {scope} {{\n" + function(source, signature) + "\n}")
    food = (ROOT / "props/prop_food_sync.cpp").read_text(encoding="utf-8-sig")
    order = (ROOT / "props/prop_food_hand_state.inc").read_text(encoding="utf-8-sig")
    bodies.extend(function(food, signature) for signature in (
        "uint32_t HandAddress(", "bool DecodeHandAddress("))
    bodies.append(function(order, "bool HandAfter("))
    main = r'''
int main(){
 constexpr uint32_t high=uint32_t{1}<<31;
 CHECK(ext::ReadyMask(session,1)&high);
 CHECK(fire::Destinations(session,1)&high);
 CHECK(grill::Mask(1)&high);
 CHECK(blast::Destinations()&high);
 CHECK(!(ext::ReadyMask(session,31)&high));
 CHECK(!(fire::Destinations(session,31)&high));
 CHECK(!(grill::Mask(31)&high));
 session.mode=net::Role::Client;CHECK(fire::Destinations(session,31)==1);
 std::set<uint32_t> addresses;
 for(uint8_t slot=0;slot<32;++slot){
  for(uint32_t generation: {1u,0x03FFFFFFu}){
   auto address=HandAddress(slot,generation);uint8_t peer=255;uint32_t decoded=0;
   CHECK(addresses.insert(address).second);
   CHECK(DecodeHandAddress(address,peer,decoded)&&peer==slot&&decoded==generation);
  }
 }
 uint8_t peer=0;uint32_t generation=0;
 CHECK(!DecodeHandAddress(0x80000000u,peer,generation));
 CHECK(HandAfter(1,0x03FFFFFFu)&&!HandAfter(0x03FFFFFFu,1));
 puts("PASS production slot-31 tool/fire/grill/explosion recipients and all held-food addresses, exclusion and wrap");
}
'''
    OUT.mkdir(parents=True, exist_ok=True)
    fixture = OUT / "peer-masks.cpp"
    fixture.write_text(PREFIX + "\n".join(bodies) + main, encoding="utf-8")
    exe = OUT / "peer-masks.exe"
    subprocess.run(["cl", "/nologo", "/std:c++20", "/EHsc", "/utf-8",
                    f"/I{CHECKOUT / 'src/votv-coop/include'}",
                    f"/Fo{OUT / 'peer-masks.obj'}", f"/Fe{exe}", str(fixture)], check=True)
    subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
