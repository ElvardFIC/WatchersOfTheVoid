#include "coop/net/protocol.h"
#include "coop/world/explosion_sync.h"
#include "ue_wrap/world/explosion.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);} } while(0)
namespace coop::native_explosion_sync {
namespace X=ue_wrap::explosion;
int applied=0,dropped=0;
DropReason last{};
void Drop(DropReason reason,const char*){++dropped;last=reason;}
void Apply(const net::ExplosionSpawnPayload&){++applied;}
namespace GT {void Post(std::function<void()> work){work();}}
#include "explosion_validation.inc"
}
int main(){
    namespace S=coop::native_explosion_sync;
    using P=coop::net::ExplosionSpawnPayload;
    P good{};good.scale[0]=good.scale[1]=good.scale[2]=1;
    good.radius=250;good.force=1000;good.damage=25;
    auto receive=[](const P& p){return S::OnSpawnReceived(reinterpret_cast<const uint8_t*>(&p),sizeof(p));};
    CHECK(receive(good)&&S::applied==1);
    good.damage=0;CHECK(receive(good)); // force-only blasts are valid
    good.classId=2;CHECK(receive(good)); // native salt-rocket variant
    good.classId=0;
    auto reject=[&](P p,S::DropReason reason){int before=S::applied;CHECK(!receive(p));CHECK(S::applied==before&&S::last==reason);};
    auto p=good;p.classId=3;reject(p,S::DropReason::Class);
    p=good;p.classId=1;reject(p,S::DropReason::Class); // personal PC minigame
    p=good;p.ignoreKind=3;reject(p,S::DropReason::IgnoreKind);
    p=good;p.flags=4;reject(p,S::DropReason::Flags);
    p=good;p.loc[1]=std::numeric_limits<float>::quiet_NaN();reject(p,S::DropReason::NonFinite);
    p=good;p.force=std::numeric_limits<float>::infinity();reject(p,S::DropReason::NonFinite);
    p=good;p.loc[0]=coop::net::kMaxCoord+100;reject(p,S::DropReason::Coord);
    p=good;p.scale[0]=0;reject(p,S::DropReason::Scale);
    p=good;p.scale[2]=100;reject(p,S::DropReason::Scale);
    p=good;p.radius=-1;reject(p,S::DropReason::Range);
    p=good;p.damage=-1;reject(p,S::DropReason::Range);
    p=good;p.fireFuel=-1;reject(p,S::DropReason::Range);
    CHECK(!S::OnSpawnReceived(nullptr,sizeof(good))&&S::last==S::DropReason::Length);
    CHECK(!S::OnSpawnReceived(reinterpret_cast<uint8_t*>(&good),sizeof(good)-1));
    CHECK(!S::OnSpawnReceived(reinterpret_cast<uint8_t*>(&good),sizeof(good)+1));
    CHECK(S::applied==3);
    std::puts("PASS actual native blast decoder: valid/force-only/salt payloads, private-PC refusal, exact length, flags, identity kinds, finite values, coordinates, scales and range rejection");
}
