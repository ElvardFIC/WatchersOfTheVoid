#pragma once
#include "coop/net/session.h"
#include "ue_wrap/core/types.h"
namespace coop::element {
inline bool headAllowed=true,tailAllowed=true;
class IntentTarget {
public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool AuthorizeSegment(const ue_wrap::FVector& point,const ue_wrap::FVector&)const{
  return point.X==10?headAllowed:tailAllowed;
 }
};
}
