#define TOWER_OPS_NO_MAIN
#include "tower_ops_test.cpp"
int main(){using namespace test;N::Session session;
 uint8_t locals[32]{};ue_wrap::script_gate::Call call{};
 auto setup=[&]{Reset(session);session.mode=N::Role::Client;S::Install(&session);paramOffset=0;look=F::Look::Pull;
  std::memset(locals,0,sizeof locals);locals[0]=4;call={};call.object=&tower;call.function=&fuse;call.locals=locals;
  call.invocation=1;call.depth=1;call.stack=&body;view.fuses[0]=0;};
 for(int cause=0;cause<9;++cause){setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);
  CHECK(S::g_clientCaptures.size()==1);
  switch(cause){case 0:++world;break;case 1:++serial;break;case 2:++session.stops;break;
   case 3:session.mode=N::Role::Host;break;case 4:++tower.serial;break;case 5:++cls.serial;break;
   case 6:++fuse.serial;break;case 7:++session.generations[0];break;case 8:session.linked=false;break;}
  S::OnUsePost(call);CHECK(session.sent.empty()&&S::g_clientCaptures.empty());
 }
 puts("PASS UI01 tower capture context/actor/class/function/host retirement");
 setup();view.fuseCount=2;CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);
 auto nested=call;nested.invocation=2;nested.depth=2;lookSlot=1;
 CHECK(S::OnUsePre(nested)==ue_wrap::script_gate::Verdict::Run);view.fuses[1]=0;
 S::OnUsePost(nested);S::OnUsePost(call);S::OnUsePost(call);
 CHECK(session.sent.size()==1&&S::g_clientCommands.size()==2);S::OnAcks(1,0);S::DrainClientCommands();
 CHECK(session.sent.size()==2&&session.sent[0].packet.index==1&&session.sent[1].packet.index==0&&S::g_clientCaptures.empty());
 setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);nested=call;++nested.invocation;
 S::OnUsePost(nested);CHECK(session.sent.empty()&&S::g_clientCaptures.size()==1);S::OnUsePost(call);CHECK(session.sent.size()==1);
 setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);nested=call;++nested.invocation;
 CHECK(S::OnUsePre(nested)==ue_wrap::script_gate::Verdict::Run&&S::g_clientCaptures.size()==1);
 S::OnUsePost(call);CHECK(session.sent.empty());S::OnUsePost(nested);CHECK(session.sent.size()==1);
 setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);hook=[](const std::string& n){if(n=="view")++serial;};
 S::OnUsePost(call);CHECK(session.sent.empty());
 for(int slot:{-1,1024}){setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);
  S::g_clientCaptures.back().slot=slot;S::OnUsePost(call);CHECK(session.sent.empty());}
 puts("PASS UI01 nested invocation matching, detached post, read continuation and slot bounds");
 for(int invalid=0;invalid<5;++invalid){setup();if(invalid==0)call.locals=nullptr;if(invalid==1)paramOffset=-1;
  if(invalid==2)paramOffset=16;if(invalid==3)paramBytes=-1;if(invalid==4){paramOffset=8;paramBytes=8;}
  CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Cancel&&S::g_clientCaptures.empty());}
 setup();CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run);S::OnUsePost(call);
 ++fuse.serial;paramOffset=8;locals[0]=0;locals[8]=4;paramBytes=9;
 CHECK(S::OnUsePre(call)==ue_wrap::script_gate::Verdict::Run&&S::g_clientCaptures.size()==1);S::OnUsePost(call);S::OnAcks(1,0);S::DrainClientCommands();CHECK(session.sent.size()==2);
 for(int offset:{-1,10,16}){setup();paramOffset=offset;CHECK(S::OnInsertPre(call)==ue_wrap::script_gate::Verdict::Cancel);}
 setup();std::memcpy(locals,&call.function,sizeof(void*));CHECK(S::OnInsertPre(call)==ue_wrap::script_gate::Verdict::Run);
 CHECK(S::g_clientCaptures.size()==1);view.fuses[0]=1;S::OnInsertPost(call);CHECK(session.sent.size()==1&&session.sent[0].packet.op==N::kFuseTowerOpInsert);
 for(const char* phase:{"view","fuseLook","difficulty"}){setup();std::memcpy(locals,&call.function,sizeof(void*));
  hook=[phase](const std::string& name){if(name==phase)++serial;};
  CHECK(S::OnInsertPre(call)==ue_wrap::script_gate::Verdict::Cancel&&S::g_clientCaptures.empty());}
 setup();S::g_claims.push_back({1,N::kFuseTowerOpInsert,F::Kind::Coordinate,0,0});S::g_seq=32768;
 CHECK(S::ClaimsFull());S::ClientSend(&session,N::kFuseTowerOpButton,F::Kind::Coordinate,0,0,false);
 CHECK(session.sent.empty()&&S::g_seq==32768);
 S::g_claims.clear();S::ClientSend(&session,N::kFuseTowerOpButton,F::Kind::Coordinate,0,0,false);
 CHECK(session.sent.size()==1&&S::g_seq==32769);
 puts("PASS UI03 unresolved claims bound the sequence window without spending or advancing a blocked request");
 puts("PASS UI02 tower parameter incarnation, null/truncated/unaligned frames and insert capture");
}
