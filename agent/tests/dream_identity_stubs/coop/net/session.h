#pragma once
#include <atomic>
namespace coop::net {
enum class Role { Host, Client };
class Session {
public:
    Role mode = Role::Host;
    Role role() const { return mode; }
};
}
