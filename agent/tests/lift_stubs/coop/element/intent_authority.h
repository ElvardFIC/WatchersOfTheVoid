#pragma once
#include "ue_wrap/core/types.h"
#include "coop/net/session.h"
namespace coop::element {
inline bool body=true,reach=true;inline ue_wrap::FVector checked;
class IntentTarget {
public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool HasBody()const{return body;}
 bool AuthorizeSegment(const ue_wrap::FVector& start,const ue_wrap::FVector&)const{checked=start;return reach;}
};
}
