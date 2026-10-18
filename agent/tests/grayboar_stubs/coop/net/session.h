#pragma once
namespace coop::net {
enum class Role {Host,Client};
struct Session {
    Role mode=Role::Host;
    bool active=true;
    Role role()const{return mode;}
    bool running()const{return active;}
};
}
