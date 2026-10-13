#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <deque>
#include "coop/net/protocol.h"
#include "coop/player/players_registry.h"
#define private public
#include "coop/net/send_rate_control.h"
#undef private
#include "../../src/votv-coop/src/coop/net/send_rate_control.cpp"
#include "../../src/votv-coop/src/coop/net/send_admission.cpp"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1); } } while(0)
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
using C=coop::net::SendRateControl;
// A deterministic finite-bandwidth pipe. Exact cumulative byte counters, delayed
// ACKs, sender pacing AND production save admission. Not a GNS/real-link benchmark.
struct Result { int ms=0;int64_t peakRate=0,peakWire=0; };
Result Transfer(int rtt,int capacity,bool interrupted=false,bool gnsPing=true) {
    C c;c.Reset(true,0);
    coop::net::SendAdmission admission;admission.Reset();admission.SetSendBufBytes(4*1024*1024);
    const int64_t total=16*1024*1024;
    int64_t handed=0,pending=0,sent=0,acked=0,wire=0,rate=C::StartRateBps();
    struct Ack { int at;int64_t bytes; };std::deque<Ack> acks;
    struct Echo { int at;coop::net::LinkProbePayload p; };std::deque<Echo> echoes;
    Result result;
    for (int t=0;t<180000;t+=10) {
        // Loss/retransmit blackout model: ACK/service pause then a temporarily
        // thinner path. Bytes remain outstanding until they really get through.
        const bool stall=interrupted && t>=6000 && t<8000;
        const int cap=interrupted && t>=8000 && t<14000 ? capacity/8 : capacity;
        if (!stall) while (!acks.empty() && acks.front().at<=t) { acked+=acks.front().bytes;acks.pop_front(); }
        if (!stall) while (!echoes.empty() && echoes.front().at<=t) {
            c.NoteProbeReply(1,echoes.front().p,uint64_t(t)+10000);echoes.pop_front();
        }
        if (acked==total) { result.ms=t;return result; }
        if (t%100==0) {
            C::LinkSample sample{};sample.pendingReliable=int(pending);sample.sentUnackedReliable=int(sent-acked);
            sample.gnsPingMs=gnsPing ? rtt+int(wire*1000/cap) : 0;sample.gnsRateBps=int(rate);
            c.Sample(1,sample,uint64_t(t)+10000);
            if (!gnsPing) {
                coop::net::LinkProbePayload probe{};
                if (c.NextProbe(1,uint64_t(t)+10000,probe)) echoes.push_back({t+rtt+int(wire*1000/cap),probe});
            }
            if (auto want=c.PendingRateWrite(1);want>0) { rate=want;c.NoteRateWritten(1,rate,0); }
            admission.Anchor(1,[&]{return int(pending);});admission.SetDrainRate(1,c.ServedBps(1));
            CHECK(rate>=C::kFloorBps && rate<=C::kCeilingBps);
            result.peakRate=std::max(result.peakRate,rate);
        }
        for (int n=0;n<4 && handed<total;++n) {
            int len=int(std::min<int64_t>(56*1024,total-handed));
            if (!admission.AdmitsPacedStream(1,len)) break;
            admission.NoteHanded(1,len);c.NoteReliableQueued(1,len);handed+=len;pending+=len;
        }
        const auto tx=std::min(pending,rate/100);pending-=tx;sent+=tx;wire+=tx;
        if (!stall) { const auto bytes=std::min<int64_t>(wire,cap/100);wire-=bytes;if (bytes) acks.push_back({t+rtt,bytes}); }
        result.peakWire=std::max(result.peakWire,wire);
    }
    return result;
}
int main() {
    for (int rtt:{30,100,200,400}) {
        auto r=Transfer(rtt,2*1024*1024);
        std::printf("16 MiB, 2 MiB/s pipe, RTT=%d ms: %.2f s, peak %.0f KiB/s\n",rtt,r.ms/1000.,r.peakRate/1024.);
        CHECK(r.ms>0 && r.ms<30000);
    }
    auto thin=Transfer(200,128*1024);CHECK(thin.ms>0 && thin.ms<150000);
    auto interrupted=Transfer(200,2*1024*1024,true);CHECK(interrupted.ms>0 && interrupted.ms<40000);
    auto echoes=Transfer(200,2*1024*1024,true,false);CHECK(echoes.ms>0 && echoes.ms<40000);
    std::printf("128 KiB/s pipe: %.2f s; stall + capacity drop/recovery: %.2f s\n",thin.ms/1000.,interrupted.ms/1000.);
    std::printf("No GNS ping, validated echoes + stall/recovery: %.2f s\n",echoes.ms/1000.);
    C c;c.Reset(true,0);auto& m=c.Take_(1);
    C::NoteBaseRtt_(m,200);C::NoteBaseRtt_(m,800);CHECK(m.baseRttMs==200);
    C::NoteBaseRtt_(m,100);C::NoteBaseRtt_(m,0);CHECK(m.baseRttMs==100);
    m.demand=100000;m.servedSamples=C::kServedWarmupSamples;m.servedBps=m.relBps=100000;
    for(auto& p:m.servedRing) p=100000;
    m.last.sentUnackedReliable=100000;m.rateBps=500000;C::Steer_(m);
    CHECK(m.brakeCount==1 && m.rateBps==100000 && m.excessMs==900);
    // A long standing queue cannot ratchet the lifetime baseline upwards.
    for (int i=0;i<200;++i) C::NoteBaseRtt_(m,1800+i);
    CHECK(m.baseRttMs==100);
    m.relBps=0;m.baseRttMs=1000;C::Steer_(m);CHECK(m.excessMs>C::kQueueBrakeMs);
    // No measured delivery is a hold, not an unbounded climb.
    m.servedBps=0;auto old=m.rateBps;C::Steer_(m);CHECK(m.rateBps==old);
    c.FreeSlot(1);CHECK(c.Take_(1).baseRttMs==-1 && c.Take_(1).rateBps==C::StartRateBps());
    C::LinkSample s{};s.gnsPingMs=200;c.Sample(1,s,10000);CHECK(c.Take_(1).baseRttMs==200);
    coop::net::LinkProbePayload probe{};CHECK(c.NextProbe(1,11000,probe));
    c.NoteProbeReply(1,probe,11150);CHECK(c.Take_(1).baseRttMs==150);
    probe.token+=100;c.NoteProbeReply(1,probe,11151);CHECK(c.Take_(1).baseRttMs==150);
    c.Reset(true,0);C::NoteBaseRtt_(c.Take_(1),5000);CHECK(c.Take_(1).baseRttMs==C::kMaxBaseRttMs);
    c.Reset(false,0);CHECK(c.PendingRateWrite(1)==-1);
    std::puts("PASS: production pacing/admission, RTT, thin link, blackout, recovery, baseline/reset/echo guards");
}
