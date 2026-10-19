#pragma once
#include "coop/element/element.h"
#include "coop/net/session.h"
namespace coop::element {
enum class IntentOutcome {Ok,OutOfReach,NoBody,NoTarget};
inline IntentOutcome testOutcome=IntentOutcome::Ok;
inline void* testTarget=nullptr;
struct IntentSubject {
 IntentOutcome outcome;void* actor;float distUU=0,reachUU=200;
 explicit operator bool()const{return outcome==IntentOutcome::Ok;}
};
class IntentTarget {
public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 IntentSubject Resolve(ElementId,ElementType)const{return {testOutcome,testTarget};}
};
inline const char* OutcomeName(IntentOutcome){return "test";}
}
