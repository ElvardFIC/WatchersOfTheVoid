#include "coop/net/intent_queue.h"
#include "ue_wrap/core/log.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
using namespace coop::net;
struct Item {
    static inline std::set<const Item*> live;
    static inline bool throwCopy=false;
    int value;
    Item(int v):value(v){live.insert(this);}
    Item(const Item& other):value(other.value){if(throwCopy)throw std::runtime_error("copy");live.insert(this);}
    Item(Item&& other) noexcept:value(other.value){live.insert(this);}
    Item& operator=(Item&& other) noexcept{value=other.value;return *this;}
    Item& operator=(const Item& other){value=other.value;return *this;}
    ~Item(){live.erase(this);}
};
int main(int argc,char** argv) {
    const std::string part=argc>1?argv[1]:"all";
    const IntentBudget one{1,0},many{10,0};
    const auto now=[] {return uint64_t{1000};};
    const auto done=[](auto&){return Served::Done;};
    if(part=="all" || part=="zero") {
        IntentBucket b;CHECK(b.Take(one,0));CHECK(!b.Take(one,0));CHECK(!b.Take(one,1));
        b.Reset();CHECK(b.Take(one,0));
    }
    if(part=="all" || part=="rewind") {
        IntentBucket b;const IntentBudget paced{1,1};CHECK(b.Take(paced,2000));
        CHECK(!b.Take(paced,1000));CHECK(!b.Take(paced,2000));CHECK(b.Take(paced,3000));
    }
    if(part=="all" || part=="ceiling") {
        IntentBucket b;CHECK(b.Take(many,1000));CHECK(b.Take(one,1000));CHECK(!b.Take(one,1000));
    }
    if(part=="all" || part=="invalid") {
        for(float value:{NAN,INFINITY,-1.f}){
            IntentBucket b;CHECK(!b.Take({value,1},1000));CHECK(!b.Take({1,value},1000));
        }
        IntentBucket b;const IntentBudget huge{2,std::numeric_limits<float>::max()};
        CHECK(b.Take(huge,0));CHECK(b.Take(huge,UINT64_MAX));CHECK(b.Take(huge,UINT64_MAX));CHECK(!b.Take(huge,UINT64_MAX));
    }
    if(part=="all" || part=="recursive") {
        IntentQueue<Item> q;CHECK(q.Push(Item{1},4));int calls=0;
        q.Serve(many,now,[&](Item&){++calls;q.Serve(many,now,[&](Item&){++calls;return Served::Wait;});return Served::Done;});
        CHECK(calls==1 && q.empty());
    }
    if(part=="all" || part=="reset_done") {
        IntentQueue<Item> q;q.Push(Item{1},4);
        q.Serve(many,now,[&](Item&){q=IntentQueue<Item>{};q.Push(Item{2},4);return Served::Done;});
        CHECK(q.size()==1);q.Serve(one,now,[&](Item& i){CHECK(i.value==2);return Served::Done;});CHECK(q.empty());
    }
    if(part=="all" || part=="payload") {
        IntentQueue<Item> q;q.Push(Item{1},4);
        q.Serve(many,now,[&](Item& item){q=IntentQueue<Item>{};CHECK(Item::live.count(&item));CHECK(item.value==1);return Served::Done;});
        CHECK(q.empty());
    }
    if(part=="all" || part=="reset_wait") {
        IntentQueue<Item> q,next;q.Push(Item{1},4);next.Push(Item{9},4);next.Serve(one,now,done);next.Push(Item{2},4);
        q.Serve(one,now,[&](Item&){q=std::move(next);return Served::Wait;});
        CHECK(q.size()==1);q.Serve(one,now,done);CHECK(q.size()==1);
    }
    if(part=="all" || part=="clock_reset") {
        IntentQueue<Item> q;q.Push(Item{1},4);int calls=0;
        q.Serve(one,[&]{q=IntentQueue<Item>{};q.Push(Item{2},4);return uint64_t{1000};},[&](Item&){++calls;return Served::Done;});
        CHECK(!calls && q.size()==1);q.Serve(one,now,done);CHECK(q.empty());
    }
    if(part=="all" || part=="exception") {
        IntentQueue<Item> q;q.Push(Item{1},4);q.Push(Item{2},4);
        try{q.Serve(many,now,[](Item&)->Served{throw std::runtime_error("native attempted");});CHECK(false);}catch(const std::runtime_error&){}
        CHECK(q.size()==1);q.Serve(many,now,[&](Item& i){CHECK(i.value==2);return Served::Done;});CHECK(q.empty());
    }
    if(part=="all" || part=="exception_guard") {
        IntentQueue<Item> q;q.Push(Item{1},4);
        try{q.Serve(one,[]()->uint64_t{throw std::runtime_error("clock");},done);CHECK(false);}catch(const std::runtime_error&){}
        q.Serve(one,now,done);CHECK(q.empty());
        q=IntentQueue<Item>{};q.Push(Item{1},4);Item::throwCopy=true;
        try{q.Serve(one,now,done);}catch(const std::runtime_error&){}
        Item::throwCopy=false;CHECK(q.size()==1);q.Serve(one,now,done);CHECK(q.empty());
    }
    if(part=="all" || part=="reset_reentry") {
        IntentQueue<Item> q;q.Push(Item{1},4);int calls=0;
        q.Serve(many,now,[&](Item&){q=IntentQueue<Item>{};q.Push(Item{2},4);q.Serve(many,now,[&](Item&){++calls;return Served::Wait;});return Served::Done;});
        CHECK(!calls && q.size()==1);
        q.Serve(many,now,done);CHECK(q.empty());
    }
    if(part=="all") {
        IntentQueue<Item> q;q.Push(Item{1},4);q.Serve(one,now,[](Item& item){item.value=9;return Served::Wait;});
        q.Serve(one,now,[](Item& item){CHECK(item.value==9);return Served::Done;});CHECK(q.empty());
        CHECK(RunIntentQueueSelftest());
    }
    std::printf("PASS intent queue/bucket %s\n",part.c_str());
}
