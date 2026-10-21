#include "saved_int_production.inc"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using namespace ue_wrap;
using namespace ue_wrap::save_record;
namespace {
struct Fixture {
    alignas(16) uint8_t record[kSaveStride]{};
    alignas(8) uint8_t baseBools[8]{1,0,1,0}, alcohol[8]{1};
    alignas(8) float baseFloats[2]{100,200}, stock[2]{38,23};
    alignas(8) int32_t soap=2;
    field_io::TArrayView bools[2]{}, floats[2]{}, ints[1]{};
    SaveRecord before, after;
    Fixture() {
        bools[0]={baseBools,4,8};bools[1]={alcohol,1,8};
        floats[0]={reinterpret_cast<uint8_t*>(baseFloats),2,2};
        floats[1]={reinterpret_cast<uint8_t*>(stock),2,2};
        ints[0]={reinterpret_cast<uint8_t*>(&soap),1,1};
        for(const auto [offset,header]:{
            std::pair{0x48,field_io::TArrayView{reinterpret_cast<uint8_t*>(bools),2,2}},
            std::pair{0x58,field_io::TArrayView{reinterpret_cast<uint8_t*>(floats),2,2}},
            std::pair{0x68,field_io::TArrayView{reinterpret_cast<uint8_t*>(ints),1,1}}})
            std::memcpy(record+offset,&header,sizeof(header));
        before.bools={{1,0,1,0},{1}};before.floats={{100,200},{38,23}};before.ints={{2}};
        after=before;after.bools[1][0]=0;after.floats[1]={0,0};after.ints[0][0]=0;
    }
    bool Apply(){return UpdateScalarGroups(record,before,after);}
    void Unchanged(){CHECK(alcohol[0]==1&&stock[0]==38&&stock[1]==23&&soap==2);}
};
}
int main(){
    Fixture f;std::array<uint8_t,kSaveStride> image{};std::memcpy(image.data(),f.record,image.size());
    const auto boolHeaders=f.bools[1],floatHeaders=f.floats[1],intHeader=f.ints[0];
    CHECK(f.Apply()&&f.alcohol[0]==0&&f.stock[0]==0&&f.stock[1]==0&&f.soap==0);
    CHECK(std::memcmp(image.data(),f.record,image.size())==0);
    CHECK(std::memcmp(&boolHeaders,&f.bools[1],sizeof(boolHeaders))==0&&
          std::memcmp(&floatHeaders,&f.floats[1],sizeof(floatHeaders))==0&&
          std::memcmp(&intHeader,&f.ints[0],sizeof(intHeader))==0);
    CHECK(f.baseBools[0]==1&&f.baseBools[2]==1&&f.baseFloats[0]==100&&f.baseFloats[1]==200);
    std::swap(f.before,f.after);CHECK(f.Apply()&&f.alcohol[0]==1&&f.stock[0]==38&&f.soap==2);
    CHECK(!UpdateScalarGroups(nullptr,f.before,f.after));
    for(int fault=0;fault<10;++fault){
        Fixture bad;
        if(fault==0)bad.ints[0].max=0;
        if(fault==1)bad.ints[0].data=reinterpret_cast<uint8_t*>(1);
        if(fault==2)bad.ints[0].num=-1;
        if(fault==3)bad.after.ints[0].push_back(3);
        if(fault==4)bad.after.floats.emplace_back();
        if(fault==5)bad.before.ints[0][0]=3;
        if(fault==6)bad.floats[1].num=1;
        if(fault==7)bad.after.bools.clear();
        if(fault==8){field_io::TArrayView invalid{nullptr,1,1};std::memcpy(bad.record+0x68,&invalid,sizeof(invalid));}
        if(fault==9)bad.ints[0].max=kMaxArr+1;
        CHECK(!bad.Apply());bad.Unchanged();
    }
    Fixture stale;stale.stock[1]=7;CHECK(!stale.Apply()&&stale.alcohol[0]==1&&stale.soap==2&&stale.stock[1]==7);
    SaveRecord empty;alignas(16) uint8_t blank[kSaveStride]{};CHECK(UpdateScalarGroups(blank,empty,empty));
    puts("PASS actual scalar-group commit: zero/refill, atomic malformed/stale refusal, unchanged base data and native array ownership");
}
