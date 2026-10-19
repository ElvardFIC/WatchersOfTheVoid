#define main baseline_main
#include "pending_birth_2130_test.cpp"
#undef main
int main(){
    baseline_main();
    using namespace test;
    Reset();floorActor=true;readable=false;Enqueue();D::Tick(&session);
    CHECK(mints==1 && session.drops.size()==1 && publishes==1 && D::g_pending.empty());
    D::Tick(&session);CHECK(session.drops.size()==1);
    // Existing/restored identity: never dispatch the mint operation.
    Reset();floorActor=true;Enqueue();D::Tick(&session);CHECK(mints==0 && session.drops.size()==1);
    for (int gate=0;gate<5;++gate) {
        Reset();floorActor=true;readable=false;
        if(gate==0)authored=false;if(gate==1)mirror=true;if(gate==2)hand=true;
        if(gate==3)tracked=true;if(gate==4){authored=false;extract=true;}
        Enqueue();for(int i=0;i<10;++i)D::Tick(&session);
        CHECK(mints==0 && session.drops.empty());
    }
    Reset();floorActor=true;readable=false;mintOk=false;Enqueue();D::Tick(&session);
    CHECK(session.drops.empty() && D::g_pending.size()==1);
    mintOk=true;D::Tick(&session);CHECK(session.drops.size()==1 && mints==2);
    Reset();floorActor=true;readable=false;mintOk=false;Enqueue();
    for(int i=0;i<30;++i)D::Tick(&session);
    CHECK(mints==9 && session.drops.empty() && D::g_pending.empty());
    for(int cause=0;cause<7;++cause){
        Reset();floorActor=true;readable=false;Enqueue();
        duringKey=[cause]{if(cause==0)++serial;if(cause==1)currentWorld=&nextWorld;
                         if(cause==2)D::Reset();if(cause==3)session.linked=false;
                         if(cause==4)mirror=true;if(cause==5)tracked=true;if(cause==6)hand=true;};
        D::Tick(&session);CHECK(session.drops.empty() && publishes==0);
    }
    Reset();floorActor=true;readable=false;Enqueue();
    duringKey=[]{D::Reset();Enqueue();};D::Tick(&session);
    CHECK(session.drops.empty() && D::g_pending.size()==1);
    D::Tick(&session);CHECK(session.drops.size()==1);
    Reset();floorActor=true;readable=false;Enqueue();
    duringKey=[]{D::Tick(&session);};D::Tick(&session);CHECK(mints==1 && session.drops.size()==1);
    puts("PASS: client floorActor native-key admission, preservation, failure retry, callback identity/reset/disconnect");
}
