#pragma once

#include <filesystem>
#include <system_error>
#include <Windows.h>

namespace ue_wrap::hotbar {

// The native importer accepts a cache solely by file count. Empty .vimg files
// decode to null but still fill its texture array. Remove only these unusable,
// regenerable files; the native count mismatch then selects its render route.
inline bool RemoveEmptyItemIcons(const std::filesystem::path& folder, unsigned& removed,
                                 std::error_code& error) {
    namespace fs = std::filesystem;
    removed = 0;
    error.clear();
    const bool exists = fs::exists(folder, error);
    if (error || !exists) return !error;
    fs::directory_iterator it(folder, error), end;
    if (error) return false;
    for (; it != end; it.increment(error)) {
        if (error) return false;
        const auto& entry = *it;
        const auto status = entry.symlink_status(error);
        if (error) return false;
        if (!fs::is_regular_file(status) || entry.path().extension() != L".vimg") continue;
        const auto size = entry.file_size(error);
        if (error) return false;
        if (size != 0) continue;
        // Deny writers while checking and deleting this exact file. Another
        // local peer may be regenerating the shared cache at the same time.
        const HANDLE file = ::CreateFileW(entry.path().c_str(), DELETE | FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            error = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
            return false;
        }
        LARGE_INTEGER length{};
        FILE_ATTRIBUTE_TAG_INFO tag{};
        bool ok = ::GetFileSizeEx(file, &length) != 0 &&
            ::GetFileInformationByHandleEx(file, FileAttributeTagInfo, &tag, sizeof(tag)) != 0;
        if (ok && length.QuadPart == 0 && !(tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            FILE_DISPOSITION_INFO disposition{TRUE};
            ok = ::SetFileInformationByHandle(file, FileDispositionInfo, &disposition, sizeof(disposition)) != 0;
            if (ok) ++removed;
        }
        if (!ok) error = std::error_code(static_cast<int>(::GetLastError()), std::system_category());
        ::CloseHandle(file);
        if (error) return false;
    }
    return !error;
}

}  // namespace ue_wrap::hotbar
