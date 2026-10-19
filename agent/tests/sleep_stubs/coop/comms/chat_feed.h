#pragma once
#include <string>
namespace coop::chat_feed {
enum class Keep { Transient };
inline void Push(const wchar_t*, Keep) {}
inline void Push(std::string, Keep) {}
}
