#include "saved_int_production.inc"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){using namespace ue_wrap::save_record;
 alignas(16) uint8_t record[kSaveStride]{};
 struct Array {void* data;int32_t num,max;} inner{},outer{};
 alignas(8) int32_t stock=5;inner={&stock,1,1};outer={&inner,1,1};std::memcpy(record+0x68,&outer,sizeof(outer));
 const auto write=[&](void* target,int value){SaveRecord before,after;before.ints={{stock}};after.ints={{value}};
  return UpdateScalarGroups(target,before,after);};
 const auto header=outer;CHECK(write(record,0)&&stock==0);
 CHECK(write(record,5)&&stock==5);CHECK(outer.data==header.data&&inner.data==&stock);
 CHECK(!write(nullptr,1));
 inner.num=0;CHECK(!write(record,2)&&stock==5);inner.num=2;inner.max=2;
 CHECK(!write(record,2)&&stock==5);inner={&stock,1,0};CHECK(!write(record,2));
 inner={reinterpret_cast<void*>(1),1,1};CHECK(!write(record,2));
 puts("PASS actual saved integer write: exact existing scalar, zero/refill, unchanged ownership, bounds and corrupt native headers");
}
