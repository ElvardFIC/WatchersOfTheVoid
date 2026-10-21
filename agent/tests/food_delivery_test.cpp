#define main PriorFoodPublisherMain
#include "food_publish_2142_test.cpp"
#undef main
void DeliveryReset(coop::net::Session& session,coop::net::Role role){
 S::OnDisconnect();session={};session.mode=role;S::g_session=&session;++foodWorld;S::EnsureWorld();
 foodReadCallback=foodWriteCallback=foodResolveCallback={};foodAuthorize=foodOutputCallback={};
 foodAlive=foodWritable=foodResolvable=true;foodHand=foodMirror=foodNative=false;
 foodLocalActor=foodRemoteActor=nullptr;foodLocalSlot=0;foodKey=L"meal";meal={300,.5f,3,10};
 foodInvocations={1};
 for(int i=0;i<4;++i)foodRoster[i].playerNo=uint16_t(10+i);
}
coop::net::FoodStatePayload Sample(float progress=20){coop::net::FoodStatePayload p{};coop::net::WireKeyFromString(L"meal",p.key);
 p.temperature=350;p.ripeness=.6f;p.uses=2;p.cookProgress=progress;return p;}
void HandReset(coop::net::Session& session,coop::net::Role role){DeliveryReset(session,role);foodLocalActor=foodRemoteActor=&actor;
 foodLocalSlot=role==coop::net::Role::Client?1:0;foodGeneration=foodMirrorGeneration=7;S::g_handKeys[1]=L"meal";S::g_handAccepted[1]={7,11,L"meal"};}
coop::net::FoodStatePayload HandSample(float progress=20){auto p=Sample(progress);p.elementId=S::HandAddress(1,7);p.playerNo=11;return p;}
int main(){coop::net::Session session;using coop::net::Role;
 for(int cause=0;cause<5;++cause){DeliveryReset(session,Role::Host);
  foodReadCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Client;if(cause==4)S::OnPeerLeft(2);};
  CHECK(!S::Publish(session,&actor,2) && session.sent.empty());
 }
 DeliveryReset(session,Role::Host);foodReadCallback=[&]{meal.cookProgress=30;CHECK(S::Publish(session,&actor,-1));};
 CHECK(!S::Publish(session,&actor,-1) && session.sent.size()==1 && S::g_sent.at(L"meal").cookProgress==30);
 DeliveryReset(session,Role::Host);session.beforeSend=[&]{meal.cookProgress=30;CHECK(S::Publish(session,&actor,-1));};
 CHECK(!S::Publish(session,&actor,-1) && session.sent.size()==2 && !S::g_sent.at(L"meal").sent);
 CHECK(S::Publish(session,&actor,-1) && session.sent.size()==3 && session.sent.back().state.cookProgress==30);
 DeliveryReset(session,Role::Host);session.beforeSend=[]{S::OnDisconnect();};CHECK(!S::Publish(session,&actor,-1) && S::g_sent.empty());
 DeliveryReset(session,Role::Host);session.beforeSend=[&]{S::OnPeerLeft(2);};CHECK(!S::Publish(session,&actor,2));
 DeliveryReset(session,Role::Client);foodWriteCallback=[]{S::Apply(Sample(30));};S::Apply(Sample());
 CHECK(S::g_pendingWorld.size()==1 && meal.cookProgress==10);S::RetryWorldConditions();
 CHECK(meal.cookProgress==30 && S::g_pendingWorld.empty() && S::g_clientHandCanonical.at(L"meal").cookProgress==30);
 DeliveryReset(session,Role::Client);foodResolveCallback=[]{S::Apply(Sample(30));};S::Apply(Sample());
 CHECK(meal.cookProgress==10 && S::g_pendingWorld.at(L"meal").state.cookProgress==30);S::RetryWorldConditions();CHECK(meal.cookProgress==30);
 for(int cause=0;cause<5;++cause){DeliveryReset(session,Role::Client);const auto before=foodWrites;
  foodWriteCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Host;if(cause==4){S::OnDisconnect();S::g_session=&session;S::EnsureWorld();}};
  S::Apply(Sample());CHECK(foodWrites==before && meal.cookProgress==10);
 }
 DeliveryReset(session,Role::Client);foodWritable=false;S::Apply(Sample());CHECK(S::g_pendingWorld.size()==1);
 foodWritable=true;S::RetryWorldConditions();CHECK(meal.cookProgress==20 && S::g_pendingWorld.empty());
 // Held-food publication binds both the source hand and the addressed recipient.
 for(int cause=0;cause<8;++cause){HandReset(session,Role::Host);
  foodReadCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Client;if(cause==4)S::OnPeerLeft(1);if(cause==5)++foodMirrorGeneration;
   if(cause==6)++foodRoster[2].playerNo;if(cause==7)S::OnPeerLeft(2);};
  CHECK(!S::BroadcastHandState(session,&actor,1,7,2) && session.sent.empty());
 }
 HandReset(session,Role::Host);foodReadCallback=[&]{meal.cookProgress=30;CHECK(S::BroadcastHandState(session,&actor,1,7));};
 CHECK(!S::BroadcastHandState(session,&actor,1,7) && session.sent.size()==1 && S::g_handSent[1].cookProgress==30);
 HandReset(session,Role::Host);session.beforeSend=[&]{meal.cookProgress=30;CHECK(S::BroadcastHandState(session,&actor,1,7));};
 CHECK(!S::BroadcastHandState(session,&actor,1,7) && session.sent.size()==2 && !S::g_handSent[1].sent);
 CHECK(S::BroadcastHandState(session,&actor,1,7) && session.sent.size()==3 && session.sent.back().state.cookProgress==30);
 HandReset(session,Role::Host);session.beforeSend=[]{S::OnDisconnect();};CHECK(!S::BroadcastHandState(session,&actor,1,7) && !S::g_handSent[1].sent);
 // A hand correction cannot finish writing after its owner, generation or role changes.
 for(int cause=0;cause<6;++cause){HandReset(session,Role::Client);const auto before=foodWrites;
  foodWriteCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Host;if(cause==4)S::OnPeerLeft(1);if(cause==5)++foodGeneration;};
  S::Apply(HandSample());CHECK(foodWrites==before && meal.cookProgress==10);
 }
 HandReset(session,Role::Client);foodWriteCallback=[]{S::Apply(HandSample(30));};S::Apply(HandSample());
 CHECK(meal.cookProgress==10 && S::g_pendingHand[1].have);S::RetryHandConditions();CHECK(meal.cookProgress==30 && !S::g_pendingHand[1].have);
 for(int cause=0;cause<7;++cause){HandReset(session,Role::Client);
  foodReadCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Host;if(cause==4)S::OnPeerLeft(1);if(cause==5)++foodGeneration;if(cause==6)foodLocalActor=nullptr;};
  S::PublishLocalHand(session,S::NowMs());CHECK(session.sent.empty());
 }
 HandReset(session,Role::Client);foodReadCallback=[&]{meal.cookProgress=30;S::PublishLocalHand(session,S::NowMs());};
 S::PublishLocalHand(session,S::NowMs());CHECK(session.sent.size()==1 && S::g_clientHandSent.at(L"meal").cookProgress==30);
 HandReset(session,Role::Client);session.beforeSend=[&]{meal.cookProgress=30;S::PublishLocalHand(session,S::NowMs());};
 S::PublishLocalHand(session,S::NowMs());CHECK(session.sent.size()==2 && !S::g_clientHandSent.at(L"meal").sent);
 S::PublishLocalHand(session,S::NowMs());CHECK(session.sent.size()==3 && session.sent.back().state.cookProgress==30);
 // A host correction delivered inside a send needs a fresh corrective owner publication.
 HandReset(session,Role::Client);session.beforeSend=[]{S::Apply(HandSample(30));};S::PublishLocalHand(session,S::NowMs());
 CHECK(meal.cookProgress==30 && !S::g_clientHandSent.at(L"meal").sent);S::PublishLocalHand(session,S::NowMs());CHECK(session.sent.back().state.cookProgress==30);
 HandReset(session,Role::Client);S::g_clientHandCanonical[L"meal"]={350,.5f,1,true,25};const auto before=foodWrites;
 foodWriteCallback=[]{++foodGeneration;};S::PublishLocalHand(session,S::NowMs());CHECK(foodWrites==before && session.sent.empty());
 uint8_t action=4;ue_wrap::script_gate::Call call{};call.object=call.function=&actor;call.locals=&action;call.invocation=1;
 // A cancelled/faulted eat cannot leave a final-bite destroy authorization behind.
 HandReset(session,Role::Client);meal.uses=1;S::OnAction(call);foodInvocations.clear();
 CHECK(!S::ConsumesClientDestroy(&actor) && S::g_bite.empty() && session.bites.empty());
 // Missing inner POST must not steal the outer observation on the same receiver.
 HandReset(session,Role::Client);meal.uses=2;S::OnAction(call);auto inner=call;inner.invocation=2;foodInvocations.insert(2);S::OnAction(inner);
 foodInvocations.erase(2);meal.uses=1;S::OnActionPost(call);CHECK(session.bites.size()==1 && S::g_bite.size()==1);
 CHECK(!S::ConsumesClientDestroy(&actor) && S::g_bite.empty());
 for(int cause=0;cause<6;++cause){HandReset(session,Role::Client);meal.uses=2;S::OnAction(call);meal.uses=1;
  foodReadCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Host;if(cause==4)S::OnPeerLeft(1);if(cause==5)foodInvocations.clear();};
  S::OnActionPost(call);CHECK(session.bites.empty());
 }
 HandReset(session,Role::Client);meal.uses=1;S::OnAction(call);session.beforeSend=[]{S::OnDisconnect();};
 CHECK(!S::ConsumesClientDestroy(&actor) && S::g_clientBiteRetry.empty() && S::g_clientBitePending.empty());
 // Nested sends retain the newer refused request, including an identical payload.
 HandReset(session,Role::Client);coop::net::FoodBitePayload bite{};coop::net::WireKeyFromString(L"meal",bite.key);
 session.beforeSend=[&]{session.sendOk=false;CHECK(S::SubmitBite(session,bite));session.sendOk=true;};
 CHECK(S::SubmitBite(session,bite) && S::g_clientBiteRetry.size()==1);const auto revision=S::g_clientBiteRetry.begin()->second.revision;
 session.beforeSend=[&]{session.sendOk=false;CHECK(S::SubmitBite(session,bite));session.sendOk=true;};S::RetryBiteRequests(session);
 CHECK(S::g_clientBiteRetry.size()==1 && S::g_clientBiteRetry.begin()->second.revision>revision);S::RetryBiteRequests(session);CHECK(S::g_clientBiteRetry.empty());
 HandReset(session,Role::Client);session.sendOk=false;for(int i=0;i<9;++i){coop::net::WireKeyFromString(L"food0"+std::to_wstring(i),bite.key);CHECK(S::SubmitBite(session,bite));}
 S::g_biteRetryCursor.clear();S::RetryBiteRequests(session);CHECK(S::g_clientBiteRetry.size()==9);session.sendOk=true;S::RetryBiteRequests(session);
 CHECK(session.bites.size()==8 && coop::net::StringFromWireKey(session.bites.front().key)==L"food08");
 for(int cause=0;cause<6;++cause){HandReset(session,Role::Host);const auto before=foodWrites;coop::net::WireKeyFromString(L"meal",bite.key);
  foodWriteCallback=[&,cause]{if(cause==0)S::OnDisconnect();if(cause==1)++foodWorld;if(cause==2)++foodSerial;
   if(cause==3)session.mode=Role::Client;if(cause==4)S::OnPeerLeft(1);if(cause==5)++foodMirrorGeneration;};
  S::OnBite(bite,1);CHECK(foodWrites==before && meal.uses==3);
 }
 // Join snapshots wait for temporary readiness and never erase a reentrant requeue.
 HandReset(session,Role::Host);foodHand=false;S::QueueFoodJoin(&actor,2);session.ready=false;S::RetryFoodJoins();CHECK(S::g_joinFoods.size()==1 && session.sent.empty());
 session.ready=true;session.beforeSend=[&]{S::QueueFoodJoin(&actor,2);};S::RetryFoodJoins();CHECK(S::g_joinFoods.size()==1);
 S::RetryFoodJoins();CHECK(S::g_joinFoods.empty() && session.sent.size()==2);
 HandReset(session,Role::Host);S::QueueHandFoodJoin(1,2);session.ready=false;S::RetryFoodJoins();CHECK(S::g_handJoin[1]);
 session.ready=true;session.beforeSend=[]{S::QueueHandFoodJoin(1,2);};S::RetryFoodJoins();CHECK(S::g_handJoin[1]);S::RetryFoodJoins();CHECK(!S::g_handJoin[1]);
 HandReset(session,Role::Host);S::QueueFoodJoin(&actor,2);S::QueueHandFoodJoin(1,2);++foodRoster[2].playerNo;S::RetryFoodJoins();
 CHECK(S::g_joinFoods.empty() && !S::g_handJoin[1] && session.sent.empty());
 HandReset(session,Role::Host);S::QueueFoodJoin(&actor,2);session.beforeSend=[&]{S::OnPeerLeft(2);S::QueueFoodJoin(&actor,2);};
 S::RetryFoodJoins();CHECK(S::g_joinFoods.size()==1);S::RetryFoodJoins();CHECK(S::g_joinFoods.empty() && session.sent.size()==2);
 // A new remote hand has no authority to publish under the preceding item's key.
 HandReset(session,Role::Host);foodGeneration=foodMirrorGeneration=8;foodKey=L"next_meal";
 CHECK(!S::BroadcastHandState(session,&actor,1,8) && session.sent.empty());auto next=HandSample(30);
 next.elementId=S::HandAddress(1,8);coop::net::WireKeyFromString(L"next_meal",next.key);S::OnHandState(next,1);
 CHECK(S::g_handAccepted[1].generation==8 && S::g_handKeys[1]==L"next_meal" && session.sent.size()==1);
 CHECK(coop::net::StringFromWireKey(session.sent.back().state.key)==L"next_meal" && session.sent.back().state.cookProgress==30);
 HandReset(session,Role::Host);foodReadCallback=[]{S::QueueHandCondition(HandSample(30),1,7);};
 CHECK(!S::BroadcastHandState(session,&actor,1,7) && session.sent.empty());S::RetryHandConditions();CHECK(meal.cookProgress==30 && session.sent.size()==1);
 puts("PASS food delivery: native-read/send supersession, fresh corrective publication, current queued receive/write, same-key nested updates and actor/recipient/session/world/role cancellation");
}
