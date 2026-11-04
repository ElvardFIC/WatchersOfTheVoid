#define CORD_CONTINUATION_NO_MAIN
#include "cord_intent_2170_test.cpp"
int main(){using namespace test;
 auto pose=[] {coop::net::PropPoseSnapshot p{};coop::net::WireKeyFromString(L"__mvcord_k0",p.key);p.holdGen=7;p.x=10;return p;};
 for(int cause=0;cause<7;++cause){Reset();objects[0].drag=true;
  callback=[cause]{if(cause==0)++world;if(cause==1)++sessionSerial;if(cause==2)++session.stops;
   if(cause==3)++objects[0].serial;if(cause==4)++objects[511].serial;if(cause==5)objects[0].drag=false;if(cause==6)objects[0].key=L"new";};
  CHECK(!S::UpdateLocalDragPose(session,false)&&!session.poseActive&&!S::g_localDragActive);
 }
 puts("PASS UI05 local head reads cannot publish through replaced context/actor/class/drag/key");
 for(int cause=0;cause<8;++cause){Host();auto p=pose();
  callback=[cause]{if(cause==0)++world;if(cause==1)++sessionSerial;if(cause==2)++session.stops;
   if(cause==3)++objects[0].serial;if(cause==4)++objects[511].serial;if(cause==5)++session.peerEpoch;
   if(cause==6)objects[0].drag=true;if(cause==7)objects[0].key=L"new";};
  S::ApplyDragPose(p,31);CHECK(writes==1&&!S::g_remoteDrags[31].actor.Get());
 }
 puts("PASS UI05 remote head completion checks sender/session/actor/class/drag/key");
 Host();auto p=pose();coop::element::authorize=[&](void*){p.holdGen=99;return true;};S::ApplyDragPose(p,31);
 CHECK(S::g_remoteDrags[31].generation==7);
 Reset();objects[0].socket=nullptr;auto first=pose();
 callback=[&]{coop::net::PropReleasePayload release{};release.key=first.key;release.holdGen=first.holdGen;CHECK(S::ApplyDragRelease(release,0));};
 S::ApplyDragPose(first,0);CHECK(S::g_remoteDrags[0].closed&&!S::g_remoteDrags[0].actor.Get());
 Reset();objects[0].socket=nullptr;first=pose();callback=[&]{auto newer=first;newer.holdGen=8;S::ApplyDragPose(newer,0);};
 S::ApplyDragPose(first,0);CHECK(S::g_remoteDrags[0].generation==8&&S::g_remoteDrags[0].actor.Get());
 puts("PASS UI05 owned pose input and nested newer pose/release receipt preservation");
}
