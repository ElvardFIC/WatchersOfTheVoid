// Real Windows codec and production CRC/header validation; no codec mocks.
#include "../../src/votv-coop/src/coop/save/save_blob_codec.cpp"
#include "../../src/votv-coop/src/coop/save/slot_file_read.cpp"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <iterator>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
using namespace coop::save_blob_codec;
using coop::slot_file_read::Crc32;
using coop::net::SaveTransferBeginPayload;
static SaveTransferBeginPayload Header(size_t wire,size_t decoded,uint32_t sidecar=0) {
    SaveTransferBeginPayload p{};p.totalBytes=uint32_t(wire);p.decodedBytes=uint32_t(decoded);p.sidecarBytes=sidecar;
    p.chunkCount=(p.totalBytes+coop::net::kSaveChunkBytes-1)/coop::net::kSaveChunkBytes;return p;
}
static void Roundtrip(const std::vector<uint8_t>& original,uint32_t sidecar=0) {
    auto wire=original;const auto crc=Crc32(original.data(),original.size());
    bool compressed=CompressIfSmaller(wire);
    const auto size=wire.size();auto h=Header(size,compressed?original.size():0,sidecar);h.crc32=crc;
    CHECK(ValidHeader(h));CHECK(DecodeAndVerify(wire,h.decodedBytes,h.crc32));CHECK(wire==original);
    std::printf("PASS roundtrip %zu -> %zu bytes, chunks=%u, sidecar=%u\n",original.size(),size,h.chunkCount,sidecar);
}
int main(int argc,char** argv) {
    // Simultaneous first call exercises thread-safe table initialization.
    std::atomic<bool> start{false};std::vector<std::thread> threads;
    for(int i=0;i<8;++i) threads.emplace_back([&]{while(!start.load())std::this_thread::yield();
        for(int j=0;j<100;++j) CHECK(Crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);});
    start=true;for(auto& t:threads)t.join();
    std::vector<uint8_t> plain(16*1024*1024,42);for(size_t i=0;i<12000;++i)plain[i]=uint8_t(i);
    Roundtrip(plain,12000);
    auto encoded=plain;CHECK(CompressIfSmaller(encoded));CHECK(encoded.size()<plain.size()/4);
    auto bad=encoded;CHECK(!DecodeAndVerify(bad,uint32_t(plain.size()+1),Crc32(plain.data(),plain.size())));CHECK(bad==encoded);
    CHECK(!DecodeAndVerify(bad,kMaxBlobBytes+1,0));CHECK(bad==encoded);
    CHECK(!DecodeAndVerify(bad,uint32_t(plain.size()),Crc32(plain.data(),plain.size())^1));CHECK(bad==encoded);
    for(int cut=1;cut<=12;++cut) {bad=encoded;bad.resize(bad.size()-cut);const auto before=bad;
        CHECK(!DecodeAndVerify(bad,uint32_t(plain.size()),Crc32(plain.data(),plain.size())));CHECK(bad==before);}
    bad=encoded;bad[bad.size()/2]^=0x80;CHECK(!DecodeAndVerify(bad,uint32_t(plain.size()),Crc32(plain.data(),plain.size())));
    std::vector<uint8_t> random(32768);uint32_t state=0x12345678;
    for(auto& b:random){state^=state<<13;state^=state>>17;state^=state<<5;b=uint8_t(state);}
    auto raw=random;CHECK(!CompressIfSmaller(raw));CHECK(raw==random);Roundtrip(raw);
    CHECK(!DecodeAndVerify(raw,0,Crc32(raw.data(),raw.size())^1));
    std::vector<uint8_t> empty;CHECK(!CompressIfSmaller(empty));CHECK(!DecodeAndVerify(empty,0,0));
    CHECK(ValidHeader({}));auto h=Header(10,100,20);CHECK(ValidHeader(h));
    auto altered=h;altered.chunkCount++;CHECK(!ValidHeader(altered));
    altered=h;altered.totalBytes=kMaxBlobBytes+1;CHECK(!ValidHeader(altered));
    altered=h;altered.decodedBytes=kMaxBlobBytes+1;CHECK(!ValidHeader(altered));
    altered=h;altered.decodedBytes=10;CHECK(!ValidHeader(altered));
    altered=h;altered.sidecarBytes=100;CHECK(!ValidHeader(altered));
    altered=h;altered.pad[0]=1;CHECK(!ValidHeader(altered));
    altered=h;altered.totalBytes=0;CHECK(!ValidHeader(altered));
    CHECK(ValidHeader(Header(1,0)));CHECK(!ValidHeader(Header(1,0,1)));
    for(int i=1;i<argc;++i) {std::ifstream f(argv[i],std::ios::binary);CHECK(f.good());
        std::vector<uint8_t> save((std::istreambuf_iterator<char>(f)),{});CHECK(!save.empty());Roundtrip(save);}
    puts("PASS: native compression/raw fallback, exact size, CRC, corruption/truncation, header bounds, concurrent CRC");
}
