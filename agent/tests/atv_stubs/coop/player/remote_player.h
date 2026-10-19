#pragma once
namespace coop {
class RemotePlayer { public:
 void* body=nullptr;
 void* GetActor()const{return body;}void* actor()const{return body;}
 bool valid()const{return body!=nullptr;}bool HasPose()const{return valid();}
};
}
