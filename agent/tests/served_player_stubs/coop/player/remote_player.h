#pragma once
#include <functional>
#include <string>
namespace coop {
namespace service_test {inline std::function<void(const std::string&)> hook;inline void Call(const char* name){auto fn=hook;if(fn)fn(name);}}
class RemotePlayer {
public:
 bool live=true,posed=true;void* body=nullptr;
 bool valid()const{service_test::Call("puppetValid");return live;}
 bool HasPose()const{service_test::Call("puppetPose");return posed;}
 void* actor()const{return GetActor();}
 void* GetActor()const{service_test::Call("puppetBody");return body;}
};
}
