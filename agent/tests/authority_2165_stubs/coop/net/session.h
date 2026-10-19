#pragma once
namespace coop::net {
enum class Role { Host, Client };
class Session {
public:
    Role mode = Role::Host;
    bool active = true;
    bool linked = true;
    bool running() const { return active; }
    bool connected() const { return linked; }
    Role role() const { return mode; }
};
}
