// coop/player/model_folder.cpp -- see coop/player/model_folder.h.

#include "coop/player/model_folder.h"

#include "coop/player/skin_registry.h"  // IsValidSkinName, BuiltinSkinPath, kNativeSkinName
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/paths.h"         // ExeDir

#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

namespace coop::model_folder {
namespace {

namespace fs = std::filesystem;

constexpr const wchar_t* kFolderName = L"multimodel";
constexpr const wchar_t* kNoteName = L"README.txt";
constexpr size_t kMaxNotes = 12;

std::mutex g_mu;
Report g_report;

// The note left in both folders, Russian and English; written once, never over a player's edit.
constexpr const char kNote[] =
    "\xEF\xBB\xBF"
    "Папка моделей игрока (MultiVoid+)\r\n"
    "\r\n"
    "Сюда кладутся готовые pak скинов Multivoid. Имя файла - это имя модели:\r\n"
    "латиница, цифры, _ и -, до 48 знаков, например hero.pak.\r\n"
    "Внутри pak: /Game/Mods/VOTVCoop/<имя>.kerfurOmega_KelSkin и tex_<имя>,\r\n"
    "сетка на скелете kerfurOmegaV1_Skeleton. Сырые .fbx, .obj, .blend игра не грузит.\r\n"
    "Рядом можно положить <имя>.png (превью в F1) и <имя>.txt (подпись, одна строка).\r\n"
    "Новый pak подхватывается на следующем запуске игры: выбери его в F1 -> Cosmetics -> Skins.\r\n"
    "По сети уходит только имя модели: у кого нет файла с тем же именем, тот видит Kel.\r\n"
    "Pak, положенный в %LOCALAPPDATA%\\MultiModel\\Models, сам копируется сюда при запуске.\r\n"
    "\r\n"
    "Player models folder (MultiVoid+)\r\n"
    "\r\n"
    "Put converted Multivoid skin paks here; the file name is the model name\r\n"
    "([A-Za-z0-9_-], up to 48 characters). Raw .fbx/.obj/.blend files are not loaded.\r\n"
    "<name>.png beside it is the preview tile, <name>.txt its title (one line).\r\n"
    "A new pak is mounted on the next game start; wear it in F1 -> Cosmetics -> Skins.\r\n"
    "Only the model name crosses the network: a player without a file of that name sees Kel.\r\n"
    "A pak put in %LOCALAPPDATA%\\MultiModel\\Models is copied here at the game's start.\r\n";

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0,
                                        nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0)
        ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr,
                              nullptr);
    return s;
}

std::wstring LowerExt(const fs::path& p) {
    std::wstring e = p.extension().wstring();
    for (wchar_t& c : e) c = static_cast<wchar_t>(::towlower(c));
    return e;
}

// The formats a player may take for a model; the game wears only a converted Multivoid skin pak.
bool IsRawModel(const std::wstring& ext) {
    static constexpr const wchar_t* kRaw[] = {
        L".fbx", L".obj", L".blend", L".gltf", L".glb", L".pmx", L".pmd", L".mdl", L".smd",
        L".vrm", L".dae", L".stl", L".psk", L".uasset", L".umap",
    };
    for (const wchar_t* r : kRaw)
        if (ext == r) return true;
    return false;
}

std::wstring GameLogicModsDir() {
    const std::wstring base = ue_wrap::paths::ExeDir();
    if (base.empty()) return {};
    return (fs::path(base).parent_path().parent_path() / L"Content" / L"Paks" / L"LogicMods").wstring();
}

// The shimloader passes the directory it maps LogicMods to on the command line; an install without
// it uses its own LogicMods.
std::wstring RealLogicModsDir() {
    int argc = 0;
    if (LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc)) {
        std::wstring dir;
        for (int i = 0; i + 1 < argc; ++i) {
            if (::_wcsicmp(argv[i], L"--pak-dir") == 0) {
                dir = argv[i + 1];
                break;
            }
        }
        ::LocalFree(argv);
        if (!dir.empty()) return dir;
    }
    return GameLogicModsDir();
}

std::wstring InboxDir() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return (fs::path(buf) / L"MultiModel" / L"Models").wstring();
}

uint32_t Fnv(uint32_t h, const unsigned char* p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

bool ComputeInfo(const fs::path& path, PakInfo& out) {
    std::error_code ec;
    const uint64_t size = fs::file_size(path, ec);
    if (ec) return false;
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    constexpr size_t kSpan = 256 * 1024;
    std::vector<unsigned char> buf(kSpan);
    uint32_t h = 2166136261u;
    auto feed = [&](uint64_t at, size_t n) {
        f.seekg(static_cast<std::streamoff>(at));
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(n));
        h = Fnv(h, buf.data(), static_cast<size_t>(f.gcount()));
        f.clear();
    };
    // The MultiModel addon's scheme, so its label and this one agree for the same file.
    if (size <= kSpan) {
        feed(0, static_cast<size_t>(size));
    } else {
        feed(0, kSpan);
        feed(size - kSpan, kSpan);
    }
    for (int i = 0; i < 8; ++i) {
        const unsigned char b = static_cast<unsigned char>((size >> (8 * i)) & 0xFF);
        h = Fnv(h, &b, 1);
    }
    char hex[9] = {};
    std::snprintf(hex, sizeof(hex), "%08x", h);
    out.size = size;
    out.fingerprint = hex;
    return true;
}

bool SameBytes(const fs::path& a, const fs::path& b) {
    PakInfo ia, ib;
    return ComputeInfo(a, ia) && ComputeInfo(b, ib) && ia.size == ib.size &&
           ia.fingerprint == ib.fingerprint;
}

// Copy through a temporary name, so a half-copied pak never sits under the real one's name.
bool CopyFresh(const fs::path& src, const fs::path& dst, std::string& err) {
    fs::path tmp = dst;
    tmp += L".copying";
    std::error_code ec;
    fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
    if (!ec) fs::rename(tmp, dst, ec);
    if (ec) {
        err = ec.message();
        std::error_code ignore;
        fs::remove(tmp, ignore);
        return false;
    }
    return true;
}

void EnsureNote(const fs::path& dir) {
    const fs::path note = dir / kNoteName;
    std::error_code ec;
    if (fs::exists(note, ec)) return;
    std::ofstream f(note, std::ios::binary);
    if (f) f.write(kNote, sizeof(kNote) - 1);
}

bool IsReservedName(const std::string& stem) {
    return stem == coop::skins::kNativeSkinName || coop::skins::BuiltinSkinPath(stem) != nullptr;
}

void AddNote(Report& r, std::string note) {
    if (r.notes.size() < kMaxNotes) r.notes.push_back(std::move(note));
}

// The folder's own files: the names the catalog will skip and the raw models it cannot use.
void ReviewFolder(const fs::path& dir, Report& r) {
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(dir, ec)) {
        if (!de.is_regular_file(ec)) continue;
        const fs::path& p = de.path();
        const std::wstring ext = LowerExt(p);
        const std::string file = Utf8(p.filename().wstring());
        if (IsRawModel(ext)) {
            AddNote(r, file + ": the game loads only a converted Multivoid skin .pak");
        } else if (ext == L".pak") {
            const std::string stem = Utf8(p.stem().wstring());
            if (!coop::skins::IsValidSkinName(stem))
                AddNote(r, file + ": a model name is A-Z, a-z, 0-9, _ and -, up to 48 characters");
            else if (IsReservedName(stem))
                AddNote(r, file + ": the name of a built-in body, so the pak is not used");
        }
    }
}

// New paks from the import folder, with the previews and titles beside them. A pak already in the
// models folder stays: the game has it mounted and open.
void Import(const fs::path& inbox, const fs::path& folder, Report& r) {
    std::error_code ec;
    for (const auto& de : fs::directory_iterator(inbox, ec)) {
        if (!de.is_regular_file(ec)) continue;
        const fs::path& src = de.path();
        const std::wstring ext = LowerExt(src);
        const std::string file = Utf8(src.filename().wstring());
        if (IsRawModel(ext)) {
            AddNote(r, file + " (import folder): the game loads only a converted Multivoid skin .pak");
            continue;
        }
        if (ext != L".pak" && ext != L".png" && ext != L".bmp" && ext != L".txt") continue;
        if (src.filename() == kNoteName) continue;
        if (ext == L".pak") {
            const std::string stem = Utf8(src.stem().wstring());
            if (!coop::skins::IsValidSkinName(stem) || IsReservedName(stem)) {
                AddNote(r, file + " (import folder): not imported -- rename it ([A-Za-z0-9_-], not a "
                               "built-in body's name)");
                continue;
            }
        }
        const fs::path dst = folder / src.filename();
        if (fs::exists(dst, ec)) {
            if (ext == L".pak" && !SameBytes(src, dst))
                AddNote(r, file + ": the models folder holds a different copy; close the game and "
                               "replace it there");
            continue;
        }
        std::string err;
        if (!CopyFresh(src, dst, err)) {
            AddNote(r, file + ": could not be copied (" + err + ")");
            continue;
        }
        if (ext == L".pak") r.imported.push_back(Utf8(src.stem().wstring()));
        UE_LOGI("model_folder: imported '%ls' into '%ls'", src.filename().c_str(), folder.c_str());
    }
}

}  // namespace

void PrepareAtBoot() {
    Report r;
    const std::wstring logicMods = RealLogicModsDir();
    if (logicMods.empty()) {
        UE_LOGW("model_folder: no LogicMods directory -- the models folder is not prepared");
        return;
    }
    const fs::path folder = fs::path(logicMods) / kFolderName;
    const std::wstring inbox = InboxDir();
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec) {
        UE_LOGW("model_folder: cannot create '%ls' (%s)", folder.c_str(), ec.message().c_str());
        return;
    }
    EnsureNote(folder);
    r.folder = folder.wstring();
    r.inbox = inbox;
    if (!inbox.empty()) {
        fs::create_directories(inbox, ec);
        if (!ec) {
            EnsureNote(inbox);
            Import(inbox, folder, r);
        }
    }
    ReviewFolder(folder, r);
    UE_LOGI("model_folder: '%ls' ready (import from '%ls'): %zu imported this start, %zu note(s)",
            r.folder.c_str(), r.inbox.c_str(), r.imported.size(), r.notes.size());
    for (const std::string& n : r.notes) UE_LOGI("model_folder: %s", n.c_str());
    std::lock_guard<std::mutex> lk(g_mu);
    g_report = std::move(r);
}

Report LastReport() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_report;
}

bool OpenInExplorer(const std::wstring& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (!fs::is_directory(dir, ec)) return false;
    // Explorer is a child process: it sees the real folder, never the shimloader's mapping.
    std::thread([dir] {
        ::ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"\"" + dir + L"\"").c_str(), nullptr,
                        SW_SHOWNORMAL);
    }).detach();
    return true;
}

const PakInfo& InfoFor(const std::wstring& pakPath) {
    static std::map<std::wstring, PakInfo> s_cache;
    auto it = s_cache.find(pakPath);
    if (it != s_cache.end()) return it->second;
    PakInfo info;
    ComputeInfo(pakPath, info);
    return s_cache.emplace(pakPath, std::move(info)).first->second;
}

std::string TitleFor(const std::wstring& dir, const std::string& skin) {
    if (dir.empty() || skin.empty()) return {};
    const fs::path p = fs::path(dir) / (std::wstring(skin.begin(), skin.end()) + L".txt");
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
        static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
        line.erase(0, 3);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
        line.pop_back();
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    line.erase(0, start);
    if (line.size() > 80) return {};
    for (unsigned char c : line)
        if (c < 0x20) return {};
    return line;
}

}  // namespace coop::model_folder
