#pragma once
#include "coop/net/session.h"
#include <functional>
#include <vector>
namespace coop::element {
inline std::function<bool(void*)> authorize;
inline std::vector<void*> checked;
class IntentTarget {
public:
 static IntentTarget ForClientIntent(net::Session&,uint8_t,float){return {};}
 bool Authorize(void* a)const{checked.push_back(a);return !authorize || authorize(a);}
};
}
