#pragma once
#include "coop/config/config_registry.h"
#include "coop/net/session.h"
namespace coop::config { bool ResolveFlag(const coop::config_registry::FlagRow&); }
namespace coop::config { float ResolveFloat(const coop::config_registry::FloatRow&); }
