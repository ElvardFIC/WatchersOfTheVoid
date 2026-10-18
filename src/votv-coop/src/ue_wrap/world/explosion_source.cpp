// ue_wrap/world/explosion_source.cpp -- see ue_wrap/world/explosion_source.h.

#include "ue_wrap/world/explosion_source.h"

namespace ue_wrap::explosion_source {

const wchar_t* ClassName(Kind k) {
    switch (k) {
        case Kind::Rocket:      return L"prop_rocket_C";
        case Kind::Sticky:      return L"prop_stickybomb_C";
        case Kind::Grenade:     return L"prop_grenade_C";
        case Kind::Pipebomb:    return L"prop_pipebomb_C";
        case Kind::Player:      return L"mainPlayer_C";
        case Kind::PoopGrenade: return L"prop_grenade_poop_C";
        case Kind::SoapGrenade: return L"prop_grenade_soap_C";
        case Kind::None:        break;
    }
    return nullptr;
}

}  // namespace ue_wrap::explosion_source
