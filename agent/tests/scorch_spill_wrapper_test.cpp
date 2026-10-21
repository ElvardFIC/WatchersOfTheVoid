#define main ExistingDrinkWrapperMain
#include "drink_spill_wrapper_test.cpp"
#undef main
int main(){using namespace test;namespace N=ue_wrap::drink_spill;auto current=[] {return true;};Reset();
    auto state=Sample();state.kind=N::kExplosionScorch;state.scale={30,40,50};state.process=25;
    CHECK(N::Valid(state));auto liquid=state;liquid.kind=0;CHECK(!N::Valid(liquid));
    auto small=state;small.scale={.001f,.001f,.001f};CHECK(N::Valid(small));small.scale.X=.0001f;CHECK(!N::Valid(small));
    auto invalid=state;invalid.scale.X=1001;CHECK(!N::Valid(invalid));invalid=state;invalid.scale.X=NAN;CHECK(!N::Valid(invalid));
    loaded=false;CHECK(!N::Author(state,true,current).attempted && !births);loaded=true;
    CHECK(N::Author(state,true,current).actor==At(Actor) && births==1 && finishes==1 && !sounds);
    CHECK(objects[Actor].cls==At(ScorchClass) && objects[Actor].process==25 && materialScale.X==30 && decalRotation.Pitch==90);
    N::State read{};uint8_t kind=99;CHECK(N::DecalKind(At(Actor),kind) && kind==N::kExplosionScorch);
    CHECK(N::Read(At(Actor),read,current) && read.kind==N::kExplosionScorch && read.scale.Z==50);
    foreignWorld=true;CHECK(!N::DecalKind(At(Actor),kind));foreignWorld=false;cdo=true;CHECK(!N::DecalKind(At(Actor),kind));cdo=false;
    CHECK(!N::SourceKind(At(Actor),kind) && !N::BreakSourceKind(At(Actor),kind) && !N::SourceClassKind(L"grime_explosionScorch_C",kind));
    objects[Projectile].grunge=At(ScorchClass);CHECK(!N::ProjectileKind(At(Projectile),kind));
    N::Launch launch{};launch.kind=N::kExplosionScorch;CHECK(!N::Valid(launch) && !N::AuthorLaunch(launch,current).attempted);
    CHECK(!N::PourSound(N::kExplosionScorch,state.location,current));
    state.process=3;CHECK(N::ApplyExisting(At(Actor),state,current) && objects[Actor].process==3 && births==1);
    for(int cause=0;cause<5;++cause){Reset();bool active=true;onApply=[cause,&active]{if(cause==0)++world;if(cause==1)++objects[Actor].serial;
        if(cause==2)++objects[ScorchClass].serial;if(cause==3)++objects[Mesh].serial;if(cause==4)active=false;};
        auto result=N::Author(state,true,[&]{return active;});CHECK(result.attempted && !result.actor && !paints);}
    puts("PASS scorch wrapper/actual ParamFrame: exact decal class/material, large native scale, current cleaning, quiet construction, no liquid source/projectile admission and native lifetime fences");return 0;
}
