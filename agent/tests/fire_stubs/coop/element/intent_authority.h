#pragma once
#include "coop/net/session.h"
#include <functional>
namespace coop::element {
inline bool body=true,reach=true;
inline std::function<void()> onProbe;
struct IntentTarget {
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool HasBody(){return body;}
 bool Authorize(void*){auto callback=std::move(onProbe);onProbe={};if(callback)callback();return reach;}
 template<class T> bool AuthorizeSegment(const T&,const T&){return reach;}
};
}
