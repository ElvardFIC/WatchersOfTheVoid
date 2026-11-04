#pragma once
#include "coop/player/remote_player.h"
#include <array>
namespace coop::players {
inline constexpr int kMaxPeers=32;
enum class PuppetLife {Alive,Dead};
class Registry {
public:
    std::array<RemotePlayer*,32> puppets{};
    std::array<PuppetLife,32> life{};
    static Registry& Get(){static Registry r;return r;}
    RemotePlayer* Puppet(int i){return puppets.at(i);}
    PuppetLife LifeOf(int i){return life.at(i);}
};
}
