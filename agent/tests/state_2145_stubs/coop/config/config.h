#pragma once
namespace coop::config_registry {struct FlagRow{};namespace rows {inline constexpr FlagRow flashlight_log{};}}
namespace coop::config {inline bool ResolveFlag(const config_registry::FlagRow&){return false;}}
