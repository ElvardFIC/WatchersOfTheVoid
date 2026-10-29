#include <string>
#pragma once
#include "coop/config/config_registry.h"
namespace coop::config {
inline std::string ResolveEnum(const config_registry::EnumRow&) { return {}; }
inline long ResolveInt(const config_registry::IntRow&) { return 0; }
inline bool ResolveFlag(const config_registry::FlagRow&) { return false; }
}
