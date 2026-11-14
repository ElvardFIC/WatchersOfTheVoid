#pragma once
namespace coop {class RemotePlayer;namespace players {
inline constexpr int kMaxPeers=32;
class Registry {public:static Registry& Get();RemotePlayer* Puppet(int)const;void* Local()const;};
}}
