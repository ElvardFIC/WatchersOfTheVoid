#include "cord_drill_detail.h"
namespace coop::dev::cord_drill::detail {
std::string Narrow(const std::wstring& w) {
    std::string out;
    out.reserve(w.size());
    for (wchar_t c : w) out.push_back(c < 128 ? static_cast<char>(c) : '?');
    return out;
}
std::wstring Wide(const std::string& s) { return std::wstring(s.begin(), s.end()); }
std::string Token(const std::string& rest, const char* tag) {
    const size_t at = rest.find(tag);
    if (at == std::string::npos) return {};
    const size_t from = at + std::char_traits<char>::length(tag);
    const size_t to = rest.find(' ', from);
    return rest.substr(from, to == std::string::npos ? std::string::npos : to - from);
}
}
