#pragma once
namespace coop::chat_feed {
enum class Keep { Transient };
inline void Push(const wchar_t*, Keep) {}
}
