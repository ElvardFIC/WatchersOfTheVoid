// Exercises the production receive state machine, codec and filesystem writer.
// Only transport/engine boundaries are doubled; no real game save directory is used.
#include "../../src/votv-coop/src/coop/save/save_transfer.cpp"
#include "../../src/votv-coop/src/coop/save/save_blob_codec.cpp"
#include "../../src/votv-coop/src/coop/save/slot_file_read.cpp"
#include <cstdlib>
#include <iterator>
#include <new>
#include <sstream>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)
namespace test {
std::filesystem::path dir;
bool failAlloc = false;
}
void* operator new(std::size_t n) {
    if (test::failAlloc) { test::failAlloc=false; throw std::bad_alloc(); }
    if (void* p=std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace ue_wrap::log { void Write(Level,const char*,...) {} }
namespace coop::save_guard { std::filesystem::path SaveGamesDir() { return test::dir; } }
namespace coop::save_identity_map {
bool DeserializeSidecar(const uint8_t*,size_t,IdMap&,size_t&) { return false; }
void LogReceivedMap(const IdMap&) {}
}
namespace coop::save_identity_bind { void SetReceivedMap(const save_identity_map::IdMap&) {} void OnDisconnect() {} }
namespace coop::join_window_baseline { void ClearForSlot(int) {} }
// Host/engine functions are link boundaries, never exercised by this receive test.
namespace coop::config_registry::detail { struct RegistryDef {
    static FlagRow Flag() { return {nullptr,RegistryCtorKey{}}; }
    static IntRow Int() { return {nullptr,RegistryCtorKey{}}; }
}; }
namespace coop::config_registry::rows {
const FlagRow save_wait_probe=detail::RegistryDef::Flag(), pile_delta_probe=detail::RegistryDef::Flag(),
              save_identity_map_log=detail::RegistryDef::Flag();
const IntRow stall_world_stream_at_pct=detail::RegistryDef::Int();
}
namespace coop::config {
bool ResolveFlag(const config_registry::FlagRow&) { std::abort(); }
long ResolveInt(const config_registry::IntRow&) { std::abort(); }
}
namespace coop::chat_feed { void Push(const std::wstring&,Keep) { std::abort(); } }
namespace coop::meadow_db_sync { void CaptureJoinSnapshot(int) { std::abort(); } void CancelJoinSnapshot(int) { std::abort(); } }
namespace coop::download_arm_sync { void CaptureJoinSnapshot(int) { std::abort(); } void CancelJoinSnapshot(int) { std::abort(); } }
namespace coop::signal_sync { void CaptureJoinSnapshot(int) { std::abort(); } void CancelJoinSnapshot(int) { std::abort(); } }
namespace coop::email_sync { void CaptureJoinSnapshot(int) { std::abort(); } void CancelJoinSnapshot(int) { std::abort(); } }
namespace coop::save_identity_map {
int BuildHostMap(IdMap&) { std::abort(); }
void SerializeSidecar(const IdMap&,std::vector<uint8_t>&) { std::abort(); }
}
namespace coop::join_beacon { void NotePhase(int,net::HostJoinPhase,uint32_t,uint32_t) { std::abort(); } }
namespace coop::join_window_baseline { void CaptureForSlot(int) { std::abort(); } void TickLateArm() { std::abort(); } }
namespace coop::save_indicator_suppress { void Begin() { std::abort(); } void End() { std::abort(); } }
namespace ue_wrap::world_identity { WorldKind CurrentWorldKind() { std::abort(); } bool Degraded() { std::abort(); } }
namespace ue_wrap::save_capture { bool CaptureLiveWorldToScratchSlot(const std::wstring&) { std::abort(); } }
namespace ue_wrap::game_mode { int ReadLocal() { std::abort(); } std::string NameOrOrdinal(int) { std::abort(); } }
namespace ue_wrap::world_singleton { void* Gamemode() { std::abort(); } }
namespace ST = coop::save_transfer;
namespace NC = coop::net;
static auto Header(const std::vector<uint8_t>& plain,const std::vector<uint8_t>& wire,bool compressed) {
    NC::SaveTransferBeginPayload p{};
    p.totalBytes=uint32_t(wire.size());p.decodedBytes=compressed?uint32_t(plain.size()):0;
    p.chunkCount=(p.totalBytes+NC::kSaveChunkBytes-1)/NC::kSaveChunkBytes;
    p.crc32=coop::slot_file_read::Crc32(plain.data(),plain.size());return p;
}
static std::vector<uint8_t> Chunk(const std::vector<uint8_t>& wire,uint32_t idx) {
    const size_t off=size_t(idx)*NC::kSaveChunkBytes;
    const size_t n=(std::min)(wire.size()-off,size_t(NC::kSaveChunkBytes));
    std::vector<uint8_t> msg(4+n);std::memcpy(msg.data(),&idx,4);std::memcpy(msg.data()+4,wire.data()+off,n);return msg;
}
static auto Dst() { return test::dir/(ST::CoopSlotName()+L".sav"); }
static auto Part() { return test::dir/(ST::CoopSlotName()+L".sav.part"); }
static std::vector<uint8_t> Read() {
    std::ifstream f(Dst(),std::ios::binary);return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});
}
static void Receive(NC::Session& s,const std::vector<uint8_t>& wire) {
    for(uint32_t i=0;size_t(i)*NC::kSaveChunkBytes<wire.size();++i) {
        auto msg=Chunk(wire,i);s.bulkSink(0,msg.data(),int(msg.size()));
    }
}
int main(int argc,char** argv) {
    CHECK(argc>=2);
    const std::string only=argc>2?argv[2]:"";
    if(only.empty()||only=="read") {
        // Tellg sees a full slot, but a native in-place rewrite truncates it
        // before read completes. Model that exact stream boundary deterministically.
        struct ShortRead : std::stringbuf {
            ShortRead():std::stringbuf(std::string(128,'X')) {}
            std::streamsize xsgetn(char* dst,std::streamsize n) override {
                return std::stringbuf::xsgetn(dst,n/2);
            }
        } shortBuf;
        std::istream input(&shortBuf);std::vector<uint8_t> result;
        CHECK(!coop::slot_file_read::ReadWholeStream_(input,result));
        if(only=="read") { puts("PASS: short read refused"); return 0; }
    }
    test::dir=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(test::dir);
    NC::Session session;session.mode=NC::Role::Client;ST::Install(&session);ST::OnDisconnect();
    ST::ClientArm();NC::SaveTransferBeginPayload noSave{};ST::OnBegin(noSave);
    CHECK(ST::GetClientState()==ST::ClientState::NoSaveAvailable);ST::OnBegin(noSave);
    CHECK(ST::GetClientState()==ST::ClientState::Failed);
    auto invalid=noSave;invalid.gameMode=255;ST::ClientArm();ST::OnBegin(invalid);
    CHECK(ST::GetClientState()==ST::ClientState::Failed);
    // Multiple chunks and exact byte-for-byte file content; the existing destination
    // is replaced on a second arm without needing a process restart.
    std::vector<uint8_t> plain(2*NC::kSaveChunkBytes+17);
    for(size_t i=0;i<plain.size();++i)plain[i]=uint8_t(i*37);
    ST::ClientArm();auto h=Header(plain,plain,false);ST::OnBegin(h);Receive(session,plain);
    CHECK(ST::GetClientState()==ST::ClientState::ReadySlotWritten);CHECK(Read()==plain);
    std::vector<uint8_t> diskRead;CHECK(coop::slot_file_read::ReadWholeFile(Dst(),diskRead));CHECK(diskRead==plain);
    auto encoded=plain;CHECK(coop::save_blob_codec::CompressIfSmaller(encoded));
    ST::ClientArm();h=Header(plain,encoded,true);ST::OnBegin(h);Receive(session,encoded);
    CHECK(ST::GetClientState()==ST::ClientState::ReadySlotWritten);CHECK(Read()==plain);
    uint32_t done=0,total=0;ST::GetProgress(done,total);CHECK(done==encoded.size()&&done==total);
    // CRC failure must never replace the last complete slot.
    ST::ClientArm();++h.crc32;ST::OnBegin(h);Receive(session,encoded);
    CHECK(ST::GetClientState()==ST::ClientState::Failed);CHECK(Read()==plain);
    ST::ClientArm();session.saveBeginSink(1,reinterpret_cast<const uint8_t*>(&h),sizeof(h));
    CHECK(ST::GetClientState()==ST::ClientState::WaitingBegin);
    session.saveBeginSink(0,reinterpret_cast<const uint8_t*>(&h),sizeof(h));
    CHECK(ST::GetClientState()==ST::ClientState::Receiving);
    ST::OnDisconnect();CHECK(!std::filesystem::exists(Dst()));
    ST::ClientArm();ST::OnBegin(Header(plain,plain,false));Receive(session,plain);
    CHECK(ST::GetClientState()==ST::ClientState::ReadySlotWritten);CHECK(Read()==plain);
    // Duplicate Begin and short/out-of-order chunks cannot produce a partial save.
    h=Header(plain,plain,false);ST::ClientArm();ST::OnBegin(h);ST::OnBegin(h);
    CHECK(ST::GetClientState()==ST::ClientState::Failed);
    ST::ClientArm();ST::OnBegin(h);auto msg=Chunk(plain,1);session.bulkSink(0,msg.data(),int(msg.size()));
    CHECK(ST::GetClientState()==ST::ClientState::Failed);
    ST::ClientArm();ST::OnBegin(h);msg=Chunk(plain,0);session.bulkSink(0,msg.data(),int(msg.size()-1));
    CHECK(ST::GetClientState()==ST::ClientState::Failed);CHECK(Read()==plain);
    // A buffered final write can fail only on close. A byte-range lock permits
    // opening/truncation but refuses writes: exercise a real Windows I/O failure.
    if(only.empty()||only=="write") {
    HANDLE lock=CreateFileW(Part().c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(lock!=INVALID_HANDLE_VALUE);OVERLAPPED ov{};
    CHECK(LockFileEx(lock,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,4096,0,&ov));
    std::vector<uint8_t> tinyBlob(31,0x42);ST::ClientArm();ST::OnBegin(Header(tinyBlob,tinyBlob,false));Receive(session,tinyBlob);
    const auto lockedState=ST::GetClientState();
    UnlockFileEx(lock,0,4096,0,&ov);CloseHandle(lock);
    CHECK(lockedState==ST::ClientState::Failed);CHECK(Read()==plain);
    // A valid fully-written .part must also not signal success if replacement
    // fails (the existing save is opened without delete/rename sharing).
    HANDLE old=CreateFileW(Dst().c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                          nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    CHECK(old!=INVALID_HANDLE_VALUE);
    ST::ClientArm();ST::OnBegin(Header(tinyBlob,tinyBlob,false));Receive(session,tinyBlob);
    const auto renameState=ST::GetClientState();CloseHandle(old);
    CHECK(renameState==ST::ClientState::Failed);CHECK(Read()==plain);
    }
    // Allocation failure on the network sink must become a failed join, not an
    // exception escaping the network thread (which would terminate the process).
    std::vector<uint8_t> allocationBlob(31,0x42);
    ST::OnDisconnect();ST::ClientArm();ST::OnBegin(Header(allocationBlob,allocationBlob,false));msg=Chunk(allocationBlob,0);
    bool escaped=false;test::failAlloc=true;
    try { session.bulkSink(0,msg.data(),int(msg.size())); } catch(const std::bad_alloc&) {escaped=true;}
    test::failAlloc=false;CHECK(!escaped);CHECK(ST::GetClientState()==ST::ClientState::Failed);
    ST::OnDisconnect();CHECK(ST::GetClientState()==ST::ClientState::Idle);ST::GetProgress(done,total);
    CHECK(done==0&&total==0);CHECK(!std::filesystem::exists(Dst()));
    // Missing LOCALAPPDATA must not silently write a relative slot in the cwd.
    const auto actualDir=test::dir;const auto cwd=std::filesystem::current_path();
    std::filesystem::current_path(actualDir);test::dir.clear();
    ST::ClientArm();ST::OnBegin(Header(allocationBlob,allocationBlob,false));Receive(session,allocationBlob);
    const auto missingDirState=ST::GetClientState();test::dir=actualDir;std::filesystem::current_path(cwd);
    CHECK(missingDirState==ST::ClientState::Failed);CHECK(!std::filesystem::exists(Dst()));ST::OnDisconnect();
    puts("PASS: production receiver, raw/compressed file, re-arm, CRC, ordering, failed file I/O, allocation failure, disconnect");
}
