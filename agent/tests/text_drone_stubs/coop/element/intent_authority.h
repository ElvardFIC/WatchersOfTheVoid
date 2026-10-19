#pragma once
#include "coop/net/session.h"
#include <functional>
namespace coop::element {
enum class IntentOutcome {Ok,Denied};
inline bool body=true,reach=true;
inline std::function<void()> callback;
class IntentTarget {
public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool HasBody()const{return body;}
 struct Result{IntentOutcome outcome;explicit operator bool()const{return outcome==IntentOutcome::Ok;}};
 Result Authorize(void*)const{auto f=std::move(callback);callback={};if(f)f();return {reach?IntentOutcome::Ok:IntentOutcome::Denied};}
};
}
