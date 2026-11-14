#pragma once
namespace coop::config_registry {
struct IntRow {};
struct FlagRow {};
struct EnumRow {};
namespace rows {
inline constexpr IntRow bind_wait_red{};
inline constexpr EnumRow prop_syncer_red{};
inline constexpr EnumRow octa_red{};
inline constexpr FlagRow bind_wait_mark_unknown{};
}
}
