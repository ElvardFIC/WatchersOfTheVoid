#pragma once
#include <cstdint>
namespace coop::net {
enum class Role : uint8_t {Host,Client};
class Session {
public:
 bool linked=true,active=true;Role mode=Role::Host;
 bool connected()const{return linked;}bool running()const{return active;}Role role()const{return mode;}
};
}
