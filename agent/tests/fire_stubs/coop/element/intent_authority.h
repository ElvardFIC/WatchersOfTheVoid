#pragma once
#include "coop/net/session.h"
namespace coop::element {
inline bool body=true,reach=true;
struct IntentTarget {
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool HasBody(){return body;}
 bool Authorize(void*){return reach;}
 template<class T> bool AuthorizeSegment(const T&,const T&){return reach;}
};
}
