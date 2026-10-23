#pragma once
#include "coop/net/session.h"
namespace coop::element {
enum class IntentOutcome {Ok,NoTarget,OutOfReach};
class IntentTarget {
public:
    static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
    bool HasBody()const{return true;}
    struct Result {IntentOutcome outcome=IntentOutcome::Ok;};
    Result Authorize(void*)const{return {};}
};
}
