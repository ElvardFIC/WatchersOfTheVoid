#define TOWER_OPS_NO_MAIN
#include "tower_ops_test.cpp"
int main(){using namespace test;N::Session session;
 auto refuse=[&]{Credit();refusal="occupied";S::HostOffer(&session,Op(1,N::kFuseTowerOpInsert),1);};
 Reset(session);spawnOk=false;spawnAttempted=false;refuse();CHECK(S::g_refundDebt.size()==1&&!S::g_refundDebt[0]->attempted);
 N::FuseTowerPayload answer{};S::FillAcks(answer,1);CHECK(answer.ack[1]==0&&answer.refused[1]==0);
 for(int i=0;i<8;++i){S::g_refundDebt[0]->retryAt=0;S::DrainRefunds();}CHECK(S::g_refundDebt.size()==1&&Count("spawn")==9);
 spawnOk=true;S::g_refundDebt[0]->retryAt=0;S::DrainRefunds();CHECK(S::g_refundDebt.empty()&&S::g_refunds==1);
 S::FillAcks(answer,1);CHECK(answer.ack[1]==1&&answer.refused[1]==1);
 puts("PASS UI03 refused pre-spawn work survives retries; acknowledgement waits for refund output");
 Reset(session);spawnOk=false;spawnAttempted=true;refuse();CHECK(S::g_refundDebt.size()==1&&S::g_refundDebt[0]->attempted);
 for(int i=0;i<8;++i){S::g_refundDebt[0]->retryAt=0;S::DrainRefunds();}CHECK(Count("spawn")==1&&S::g_refundDebt.size()==1);
 S::FillAcks(answer,1);CHECK(answer.ack[1]==0&&answer.refused[1]==0);
 puts("PASS UI03 uncertain native spawn remains recorded without repeating it");
 Reset(session);spawnOk=false;spawnAttempted=false;refuse();session.ready[1]=false;
 S::Tick();CHECK(S::g_refundDebt.size()==1&&Count("spawn")==1);
 spawnOk=true;session.ready[1]=true;S::g_refundDebt[0]->retryAt=0;S::Tick();CHECK(S::g_refundDebt.empty()&&S::g_refunds==1);
 puts("PASS UI03 temporary world-ready loss retains refund and refusal ownership");
 for(int cause=0;cause<4;++cause){Reset(session);spawnOk=false;spawnAttempted=false;refuse();
  if(cause==0)++world;if(cause==1)++serial;if(cause==2)++session.generations[1];if(cause==3)++session.stops;
  S::DrainRefunds();CHECK(S::g_refundDebt.empty()&&Count("spawn")==1);}
 Reset(session);Credit();writeOk=false;S::HostOffer(&session,Op(1,N::kFuseTowerOpInsert),1);
 CHECK(S::g_taken==0&&S::g_refusedCount==1&&S::g_refunds==1&&Count("insert")==1&&Count("publish")==0);
 S::DrainRefunds();CHECK(Count("insert")==1&&Count("spawn")==1);
 puts("PASS UI03 output confirmation precedes taken accounting; refund retries never replay insert");
}
