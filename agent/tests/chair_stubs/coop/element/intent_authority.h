#pragma once
#include "coop/net/session.h"
#include <functional>
namespace coop::element {
enum class IntentOutcome {Ok,Denied,NoBody,NoTarget};
inline bool body=true,reach=true,target=true;
inline std::function<void()> callback;
class IntentTarget {public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 struct Result{IntentOutcome outcome;explicit operator bool()const{return outcome==IntentOutcome::Ok;}};
 Result Authorize(void*)const{auto copy=std::move(callback);callback={};if(copy)copy();
  return {!body?IntentOutcome::NoBody:!target?IntentOutcome::NoTarget:reach?IntentOutcome::Ok:IntentOutcome::Denied};}
};}
