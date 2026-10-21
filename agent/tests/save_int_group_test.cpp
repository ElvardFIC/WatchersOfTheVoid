#include "saved_int_production.inc"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){using namespace ue_wrap::save_record;
 alignas(16) uint8_t record[kSaveStride]{};
 struct Array {void* data;int32_t num,max;} inner{},outer{};
 alignas(8) int32_t stock=5;inner={&stock,1,1};outer={&inner,1,1};std::memcpy(record+0x68,&outer,sizeof(outer));
 const auto header=outer;CHECK(WriteSingleIntGroup(record,0,0)&&stock==0);
 CHECK(WriteSingleIntGroup(record,0,5)&&stock==5);CHECK(outer.data==header.data&&inner.data==&stock);
 CHECK(!WriteSingleIntGroup(nullptr,0,1)&&!WriteSingleIntGroup(record,1,1)&&!WriteSingleIntGroup(record,64,1));
 inner.num=0;CHECK(!WriteSingleIntGroup(record,0,2)&&stock==5);inner.num=2;inner.max=2;
 CHECK(!WriteSingleIntGroup(record,0,2)&&stock==5);inner={&stock,1,0};CHECK(!WriteSingleIntGroup(record,0,2));
 inner={reinterpret_cast<void*>(1),1,1};CHECK(!WriteSingleIntGroup(record,0,2));
 puts("PASS actual saved integer write: exact existing scalar, zero/refill, unchanged ownership, bounds and corrupt native headers");
}
