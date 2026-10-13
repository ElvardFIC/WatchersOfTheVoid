#include "support_2135.h"
#include "../../src/votv-coop/src/coop/player/sit_pose.cpp"
#include <limits>
namespace S=coop::sit_pose;
namespace test {void* seat=nullptr;float chairYaw=0,applied=0;int rotations=0;bool chair=true;}
namespace ue_wrap::reflection {std::wstring ClassNameOf(void*){return test::chair?L"prop_sitOfficeChair_C":L"prop_box_C";}}
namespace ue_wrap::prop {bool IsDescendantOfProp(void*){return true;}std::wstring GetInteractableKeyString(void*){return L"chair";}}
namespace ue_wrap::engine {
bool ReadMainPlayerSittingOn(void*,void*& out){out=test::seat;return true;}
bool TryGetActorLocation(void*,FVector& out){out={};return true;}
bool TryGetActorRotation(void*,FRotator& out){out={0,test::chairYaw,0};return true;}
bool SetActorRotation(void*,const FRotator& r){test::applied=r.Yaw;++test::rotations;return true;}
bool SetActorLocation(void*,const FVector&){return true;}
FVector GetComponentLocation(void*){return {};}
FRotator GetComponentWorldRotation(void*){return {};}
FVector GetActorForwardVector(void*){return {1,0,0};}
}
namespace ue_wrap::sit_pose {
void DriveSitting(void*,bool){}void ClearMeshShift(void*){}void ShiftMesh(void*,float,float){}
bool IsSittingOnSomething(void*){return test::seat!=nullptr;}
float ModelHead(void*){return 20;}float ModelHalfLength(void*){return 80;}
void* AtvDriverSeat(void*){return nullptr;}
}
namespace ue_wrap::atv {bool EnsureResolved(){return false;}bool IsAtv(void*){return false;}}
namespace coop::atv_sync {bool LocalIsDriving(){return false;}int CopyIndexed(void**,uint8_t*,int){return 0;}}
namespace coop::puppet_drive {coop::RemotePlayer& Puppet(int){return coop::players::Registry::Get().puppet;}}
namespace coop::prop_element_tracker {void* ResolveLiveActorByKey(const std::wstring& key,bool*){return key==L"chair"?&test::actor:nullptr;}}
int main(){using namespace test;coop::net::Session session;S::Install(&session);seat=&actor;chairYaw=65;
 S::Tick();CHECK(session.sent.size()==1);coop::net::SitAnchorPayload p{};std::memcpy(&p,session.sent.back().data.data(),sizeof(p));
 CHECK(p.active&&p.chairKey.len&&p.chairYaw==65);seat=nullptr;S::Tick();CHECK(session.sent.size()==2);
 std::memcpy(&p,session.sent.back().data.data(),sizeof(p));CHECK(!p.active&&p.chairYaw==65);
 coop::players::Registry::Get().peer=0;S::OnAnchor(1,p);CHECK(rotations==1&&applied==65);
 coop::element::reach=false;S::OnAnchor(1,p);CHECK(rotations==1);coop::element::reach=true;
 chair=false;S::OnAnchor(1,p);CHECK(rotations==1);chair=true;
 seat=&actor;S::OnAnchor(1,p);CHECK(rotations==1);seat=nullptr;
 p.chairYaw=std::numeric_limits<float>::quiet_NaN();S::OnAnchor(1,p);CHECK(rotations==1);
 puts("PASS chair pose and final stand-up rotation, host reach, non-chair, local occupant and malformed payload guards");
}
