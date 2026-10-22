#pragma once
#include "ue_wrap/core/types.h"
#include <functional>
namespace coop::net {class Session;}
namespace coop::element {
inline bool bloodAllowed=true;
inline std::function<void()> bloodAuthorize;
class IntentTarget {
public:
    static IntentTarget ForClientIntent(net::Session&,int,float){return {};}
    bool AuthorizeSegment(const ue_wrap::FVector&,const ue_wrap::FVector&)const{
        auto fn=std::move(bloodAuthorize);bloodAuthorize={};if(fn)fn();return bloodAllowed;
    }
};
}
