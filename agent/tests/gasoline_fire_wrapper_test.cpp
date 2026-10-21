#define PROP_FIRE_WRAPPER_FIXTURE_ONLY
#include "prop_fire_wrapper_2137_test.cpp"
int main(){
    const auto setup=[] {Reset();actor.cls=&objects[8];};
    setup();CHECK(F::IsGasoline(&actor) && !F::IsProp(&actor));
    CHECK(Apply({true,30,.25f}) && ignites==1 && lastFunction==5 && flame.speed==.25f);
    CHECK(F::Owner(&flame)==&actor);CHECK(Apply({true,20,.25f}) && ignites==1 && flame.fuel==20);
    std::vector<ue_wrap::CachedObjRef> rows;F::CollectBurning(rows);CHECK(rows.size()==1 && rows[0].Is(&actor));
    CHECK(Apply({false,0,.25f}) && extinguishes==1);
    setup();actor.cls=&objects[9];CHECK(!F::IsGasoline(&actor) && !Apply() && !calls);
    for(int cause=0;cause<4;++cause){setup();callback=[cause]{if(cause==0)++world;if(cause==1)++actor.serial;if(cause==2)current=false;if(cause==3)actor.cls=&objects[9];};CHECK(!Apply());}
    setup();CHECK(F::Ignite(&actor,30) && ignites==1 && lastFunction==5);flame.owner=nullptr;F::State state{};CHECK(!F::Read(&actor,state));
    setup();CHECK(!Apply({true,NAN,.25f}) && !Apply({true,30,-1}) && !calls);
    for(int cause=0;cause<5;++cause){setup();CHECK(Apply());const float prior=flame.fuel;
        int reads=0;metadata=[cause,&reads](const wchar_t* name){if(wcscmp(name,L"fuelSpeed") || ++reads!=2)return;metadata={};
            if(cause==0)++world;if(cause==1)++actor.serial;if(cause==2)++flame.serial;if(cause==3)current=false;if(cause==4)actor.cls=&objects[9];};
        CHECK(!Apply({true,10,.25f}) && flame.fuel==prior);}
    setup();bool attempted=true;bind=false;CHECK(!F::Ignite(&actor,30,&attempted) && !attempted && !calls);
    setup();complete=false;CHECK(!F::Ignite(&actor,30,&attempted) && attempted && flame.fuel==30 && ignites==1);
    complete=true;CHECK(F::Extinguish(&actor,&attempted) && attempted && !actor.flame);
    CHECK(F::Extinguish(&actor,&attempted) && !attempted);
    setup();flameField=false;CHECK(!F::Extinguish(&actor,&attempted) && !attempted && !calls);
    CHECK(!F::Read(&actor,state) && !Apply({false,0,.25f}));flameField=true;CHECK(F::Extinguish(&actor,&attempted) && !attempted);
    puts("PASS gasoline fire wrapper/actual ParamFrame: exact native gasoline initialization, preserved component parameters, no prop body, bounded fuel, ownership and native interruption");return 0;
}
