#pragma once
#include "coop/player/players_registry.h"
namespace coop::puppet_drive { inline RemotePlayer& Puppet(int slot) { return players::Registry::Get().puppets[slot]; } }
