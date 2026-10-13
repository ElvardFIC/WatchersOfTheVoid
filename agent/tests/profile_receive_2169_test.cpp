#include "../../src/votv-coop/src/coop/items/player_inventory_sync.cpp"
#include "../../src/votv-coop/src/coop/items/inventory_wire.cpp"
#include "../../src/votv-coop/src/coop/items/save_record_wire.cpp"
#include "../../src/votv-coop/src/coop/net/blob_chunks.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <functional>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
namespace test {
namespace P=coop::player_inventory_sync;
coop::net::Session session;
std::array<std::string,4> guids{"host","alice","bob","carol"};
std::wstring nick=L"player";
std::map<std::string,std::vector<uint8_t>> stored;
int storeWrites=0;
ue_wrap::inventory::PlayerInventory localItems;
bool materialReady=true;int liveUnits=0;uint32_t world=1;
int pawnSerial=1,rotationReads=0;
std::function<void()> onPose,onVitals;
std::vector<uint8_t> Blob(int units){
 coop::player_profile::Profile p;ue_wrap::inventory::EquipRecord e;
 e.propName=L"wallfixer";e.propKey=e.data.key=L"tool_1";e.data.className=L"prop_wallfixer_C";e.data.ints={{units}};
 e.data.strings={{std::wstring(500,L'x')}}; // exercise multi-part profiles, not just tiny packets
 p.items.hold.push_back(e);return coop::inventory_wire::Serialize(p);
}
std::vector<coop::net::BlobChunkPayload> Chunks(const std::vector<uint8_t>& blob,uint32_t seq){
 coop::net::Session sender;CHECK(coop::blob_chunks::SendBlob(&sender,coop::net::ReliableKind::PlayerInventoryBlob,seq,blob));
 return sender.sent;
}
void Send(const std::vector<uint8_t>& blob,uint32_t seq,uint8_t slot=1){for(const auto& p:Chunks(blob,seq))P::OnReliable(p,slot);}
void Reset(){P::OnDisconnect();session={};P::g_session=&session;guids={"host","alice","bob","carol"};stored.clear();storeWrites=0;
 localItems={};materialReady=true;liveUnits=0;world=1;onPose={};onVitals={};coop::players::local=nullptr;pawnSerial=1;rotationReads=0;}
}
namespace ue_wrap::log {void Write(Level,const char*,...){}}
namespace ue_wrap::inventory {
void* ResolveSaveSlot(){std::abort();} bool ReadAll(PlayerInventory& out){out=test::localItems;return true;}
bool ApplyToSaveObject(void*,const PlayerInventory&){std::abort();}
}
namespace ue_wrap::vitals {
bool ReadSnapshot(Snapshot&){if(test::onVitals)test::onVitals();return false;}bool ReadDefaults(Snapshot&){std::abort();}
bool ApplySnapshot(void*,const Snapshot&){std::abort();}bool WritePlayerTransform(void*,float,float,float,float){std::abort();}
}
namespace ue_wrap::engine {
bool ReadMainPlayerRagdollState(void*,bool& r,bool& d){r=d=false;return true;}bool ReadMainPlayerSittingOn(void*,void*& seat){seat=nullptr;return true;}
bool TryGetActorLocation(void*,FVector& out){out={};if(test::onPose)test::onPose();return true;}bool TryGetActorRotation(void*,FRotator& out){++test::rotationReads;out={};return true;}
void SetSaveObjectReadyHook(void(*)(void*)){}
}
namespace ue_wrap::world_identity {uint32_t Generation(){return test::world;}void* WorldOf(void*){return nullptr;}void* CurrentWorld(){return nullptr;}}
namespace ue_wrap::reflection {
int32_t InternalIndexOf(void*){return 1;}int32_t AllocateSlotSerial(int32_t){return test::pawnSerial;}
int32_t SlotSerial(int32_t){return test::pawnSerial;}bool IsLiveByIndex(void* p,int32_t i){return p==coop::players::local&&i==1;}
}
namespace ue_wrap::puppet {bool ReadCharacterIsFalling(void*){return false;}}
#ifndef CANNED_PROFILE_TEST
namespace ue_wrap::canned_food {bool RefreshHeldRecord(void*,save_record::SaveRecord&){return true;}}
#endif
namespace ue_wrap::building_material {bool RefreshHeldMaterialRecord(void*,save_record::SaveRecord& record){
 if(!test::materialReady)return false;record.ints={{test::liveUnits}};return true;}}
namespace ue_wrap::begin_equipment {bool GiveFromClass(const std::wstring&){std::abort();}}
namespace ue_wrap::save_capture {
void SetWorldGatherHook(void(*)()){}bool InstallGatherWatch(){std::abort();}EventState GameEventState(){std::abort();}
}
namespace ue_wrap::save_to_slot_hook {void SetWritten(void(*)(void*,const wchar_t*)) {}}
namespace coop::hand_item {void* LocalHandActor(){return &test::liveUnits;}}
namespace coop::prop_synth_key {std::wstring RandomKeyString(){std::abort();}}
namespace coop::player_handshake {
const std::string& GuidForSlot(int slot){return test::guids.at(slot);}
const std::wstring& NicknameForSlot(int){return test::nick;}
}
namespace coop::player_profile_store {
std::string NickForJson(const std::wstring&){return "player";}
bool Put(const std::string& guid,std::vector<uint8_t> blob,const std::string&){test::stored[guid]=std::move(blob);++test::storeWrites;return true;}
Found Get(const std::string&,std::vector<uint8_t>&){std::abort();}void Quarantine(const std::string&){std::abort();}
bool AnythingPending(){std::abort();}void MarkWorldGathered(){std::abort();}size_t CutToDisk(const std::wstring&){std::abort();}
}
namespace coop::signal_wire {
std::vector<uint8_t> Serialize(const ue_wrap::signal_dynamic::Row&){std::abort();}
bool Deserialize(const std::vector<uint8_t>&,ue_wrap::signal_dynamic::Row&){std::abort();}
}
int main(){using namespace test;
 const auto full=Blob(5),empty=Blob(0);CHECK(Chunks(full,1).size()>1);
 Reset();Send(full,10);CHECK(storeWrites==1&&stored["alice"]==full);
 Send(empty,11);Send(full,10);Send(full,11);CHECK(storeWrites==2&&stored["alice"]==empty);
 // Invalid high sequences do not overwrite the last good profile or fence out valid retries.
 auto bad=full;bad.pop_back();Send(bad,100);Send(empty,12);CHECK(storeWrites==3&&stored["alice"]==empty);
 bad=full;bad.push_back(0);Send(bad,101);bad=full;bad[0]=1;Send(bad,102);CHECK(storeWrites==3);
 Send(full,13);CHECK(storeWrites==4&&stored["alice"]==full);
 // Newer partial snapshots survive an older successful snapshot; older partials cannot roll back.
 Reset();auto old=Chunks(full,10),fresh=Chunks(empty,11);P::OnReliable(fresh[0],1);
 for(const auto& p:old)P::OnReliable(p,1);
 for(size_t i=1;i<fresh.size();++i)P::OnReliable(fresh[i],1);
 CHECK(storeWrites==2&&stored["alice"]==empty);
 Reset();P::OnReliable(old[0],1);Send(empty,11);
 for(size_t i=1;i<old.size();++i)P::OnReliable(old[i],1);CHECK(storeWrites==1&&stored["alice"]==empty);
 // Slot reuse cannot splice two occupants' chunks, even if GUID changed without a leave callback.
 Reset();P::OnReliable(old[0],1);guids[1]="replacement";
 for(size_t i=1;i<old.size();++i)P::OnReliable(old[i],1);CHECK(stored.empty());
 Send(empty,1);CHECK(stored["replacement"]==empty);
 Reset();P::OnReliable(old[0],1);P::OnDisconnectForSlot(1);
 for(size_t i=1;i<old.size();++i)P::OnReliable(old[i],1);CHECK(stored.empty());
 Send(empty,1);CHECK(storeWrites==1);
 // Other peers' partials and saved profiles survive an unrelated slot disconnect.
 Reset();P::OnReliable(old[0],2);Send(full,9,1);P::OnDisconnectForSlot(1);
 for(size_t i=1;i<old.size();++i)P::OnReliable(old[i],2);
 CHECK(stored["alice"]==full&&stored["bob"]==full);
 // Reject before buffering: anonymous, disconnected, stopped, host/self and out-of-range slots.
 Reset();guids[1].clear();P::OnReliable(old[0],1);guids[1]="alice";
 for(size_t i=1;i<old.size();++i)P::OnReliable(old[i],1);CHECK(stored.empty());
 session.slots[1]=false;Send(full,1);session.slots[1]=true;
 session.started=false;Send(full,2);session.started=true;session.linked=false;Send(full,3);session.linked=true;
 Send(full,4,0);Send(full,5,4);CHECK(stored.empty());
 // Wrap ordering and disconnect reset: a low sequence after reconnect belongs to the new stream.
 Reset();Send(full,0xfffffffeu);Send(empty,0xffffffffu);Send(full,0);Send(empty,0xffffffffu);CHECK(storeWrites==3&&stored["alice"]==full);
 P::OnDisconnectForSlot(1);Send(empty,0);CHECK(storeWrites==4&&stored["alice"]==empty);
 P::OnDisconnect();CHECK(stored["alice"]==empty&&P::g_assembler.Idle());
 // Join apply has its own ordering and closes after application; no client can replace it.
 Reset();session.mode=coop::net::Role::Client;Send(full,1,1);CHECK(!P::HasPendingApply());
 Send(full,8,0);Send(empty,9,0);Send(full,8,0);CHECK(P::HasPendingApply()&&P::g_pendingApply.items.hold[0].data.ints[0][0]==0);
 Send(bad,20,0);Send(full,10,0);CHECK(P::g_pendingApply.items.hold[0].data.ints[0][0]==5);
 P::g_profileApplied=true;Send(empty,11,0);CHECK(P::g_pendingApply.items.hold[0].data.ints[0][0]==5);
 P::OnDisconnect();CHECK(!P::HasPendingApply());Send(empty,1,0);CHECK(P::HasPendingApply());
 // The two flag bytes are canonical booleans; legacy projection migration consumes the whole blob.
 coop::player_profile::Profile decoded;auto flags=coop::inventory_wire::Serialize({});
 flags[13]=2;CHECK(!coop::inventory_wire::Deserialize(flags,decoded));flags[13]=0;flags[14]=2;CHECK(!coop::inventory_wire::Deserialize(flags,decoded));
 auto legacy=coop::inventory_wire::SerializeItems({});legacy[0]=1;uint8_t version=0;
 CHECK(coop::inventory_wire::Deserialize(legacy,decoded,&version)&&version==1);legacy.push_back(0);CHECK(!coop::inventory_wire::Deserialize(legacy,decoded,&version));
 // Full stream integration: refresh before hashing, including zero; a failed hand read/send retries.
 Reset();session.mode=coop::net::Role::Client;P::g_profileApplied=true;
 CHECK(coop::inventory_wire::Deserialize(full,decoded));localItems=decoded.items;liveUnits=0;
 auto poll=[] {P::g_lastPoll={};P::ClientStreamTick(&session);};
 materialReady=false;poll();CHECK(session.sent.empty());materialReady=true;poll();CHECK(!session.sent.empty());
 coop::blob_chunks::Assembler receive;std::vector<uint8_t> result;
 for(const auto& part:session.sent)receive.OnChunk(part,1,result);
 CHECK(coop::inventory_wire::Deserialize(result,decoded)&&decoded.items.hold[0].data.ints[0][0]==0);
 CHECK(localItems.hold[0].data.ints[0][0]==5);session.sent.clear();poll();CHECK(session.sent.empty());
 liveUnits=2;session.sendOk=false;poll();CHECK(session.sent.empty());session.sendOk=true;poll();CHECK(!session.sent.empty());
 // An engine read that disconnects or travels must not emit/commit a profile from the old context.
 for(int reason=0;reason<3;++reason){Reset();session.mode=coop::net::Role::Client;P::g_profileApplied=true;localItems=decoded.items;
  coop::players::local=&session;
  if(reason==0)onPose=[]{P::OnDisconnect();};
  if(reason==1)onPose=[]{++world;};
  if(reason==2)onVitals=[]{P::OnDisconnect();};
  poll();CHECK(session.sent.empty()&&P::g_lastItemsHash==0);
  if(reason<2)CHECK(rotationReads==0&&!P::g_standingPose.valid);
 }
 Reset();coop::players::local=&session;onPose=[]{++pawnSerial;};P::SampleStandingPose();CHECK(rotationReads==0&&!P::g_standingPose.valid);
 std::puts("PASS real profile receiver/codec/chunker: current stock, malformed/stale/duplicate snapshots, partial ordering, slot reuse, role guards, wrap and join apply");
 return 0;
}
