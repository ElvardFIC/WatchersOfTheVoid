#define main PriorSpongeWrapperMain
#include "sponge_effects_wrapper_test.cpp"
#undef main
int main(){using namespace test;namespace FX=ue_wrap::sponge_effects;
    for(int cause=0;cause<7;++cause){
        onLookup=onCall=onStock=onFunction={};alternateSchema=false;fields=stock=true;
        for(int i=0;i<Count;++i){const auto serial=objects[i].serial+1;objects[i]={};objects[i].index=i;objects[i].serial=serial;}
        objects[Sponge].player=At(Player);objects[Sponge].soap=100;
        FX::Use out{};out.soap=-9;out.location={-1,-2,-3};
        onStock=[cause]{
            if(cause==0)++objects[SpongeClass].serial;if(cause==1)++objects[PlayerClass].serial;
            if(cause==2)++objects[HitStruct].serial;if(cause==3)objects[Sponge].player=nullptr;
            if(cause==4)alternateSchema=true;if(cause==5)++objects[Sponge].serial;if(cause==6)++objects[Player].serial;
        };
        CHECK(!FX::ReadUse(At(Sponge),At(Player),out) && out.soap==-9 && out.location.X==-1);
    }
    alternateSchema=false;objects[Sponge].player=At(Player);FX::Use out{};
    onLookup=[]{++objects[HitStruct].serial;};CHECK(!FX::ReadUse(At(Sponge),At(Player),out));
    CHECK(FX::ReadUse(At(Sponge),At(Player),out) && out.location.X==100 && out.soap==100);
    puts("PASS sponge trace: actor/player/class/schema retirement, exact player link and struct binding, unchanged failure output and recovery");
}
