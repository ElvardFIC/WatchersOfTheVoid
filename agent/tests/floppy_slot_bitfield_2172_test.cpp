#include "../../src/votv-coop/src/ue_wrap/devices/floppy_slot.cpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { \
    std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::exit(1); \
} } while (0)

namespace test {
struct Device {
    int32_t type = 2;
    int32_t readWrites = 4;
    ue_wrap::field_io::TArrayView data{};
    ue_wrap::field_io::FStringView objectData{};
    ue_wrap::field_io::FStringView nametype{};
    void* widget = nullptr;
    uint8_t flags = 0;
} device;

bool provideZipProperty = true;
uint8_t zipMask = 0x04;
int zipOffsetLookups = 0;
int boolPropertyLookups = 0;
int classToken = 0;
}

namespace ue_wrap::reflection {
void* FindClass(const wchar_t*) { return &test::classToken; }
void* FindClassDefaultObject(const wchar_t*) { return nullptr; }
void* FindFunction(void*, const wchar_t*) { return nullptr; }

int32_t FindPropertyOffset(void*, const wchar_t* name) {
    if (std::wcscmp(name, L"zip") == 0) {
        ++test::zipOffsetLookups;
        return offsetof(test::Device, flags);
    }
    if (std::wcscmp(name, L"floppyType") == 0) return offsetof(test::Device, type);
    if (std::wcscmp(name, L"floppyReadwrites") == 0)
        return offsetof(test::Device, readWrites);
    if (std::wcscmp(name, L"floppyData") == 0) return offsetof(test::Device, data);
    if (std::wcscmp(name, L"floppyObjectData") == 0)
        return offsetof(test::Device, objectData);
    if (std::wcscmp(name, L"floppyNametype") == 0)
        return offsetof(test::Device, nametype);
    if (std::wcscmp(name, L"Widget") == 0) return offsetof(test::Device, widget);
    return -1;
}

bool FindBoolProperty(void*, const wchar_t* name, int32_t& offset, uint8_t& mask) {
    ++test::boolPropertyLookups;
    CHECK(std::wcscmp(name, L"zip") == 0);
    if (!test::provideZipProperty) return false;
    offset = offsetof(test::Device, flags);
    mask = test::zipMask;
    return true;
}
}

namespace ue_wrap {
ParamFrame::ParamFrame(void* fn) : fn_(fn) {}
bool ParamFrame::SetRaw(const wchar_t*, const void*, int32_t) { return false; }
bool ParamFrame::GetRaw(const wchar_t*, void*, int32_t) const { return false; }
bool Call(void*, ParamFrame&) { return false; }
}

namespace ue_wrap::component_calls {
bool CallParamlessNamed(void*, const wchar_t*) { return true; }
}

namespace ue_wrap::engine {
bool SetStaticMesh(void*, void*) { return true; }
bool ClearStaticMesh(void*) { return true; }
}

namespace ue_wrap::field_io {
std::wstring ReadFStringAt(const void*, int32_t) { return {}; }
bool WriteFStringField(void*, int32_t, const std::wstring&) { return true; }
std::vector<std::wstring> ReadFStringArrayField(const void*, int32_t) { return {}; }
bool WriteFStringArrayField(void*, int32_t, const std::vector<std::wstring>&) {
    return true;
}
std::vector<int32_t> ReadInt32ArrayField(const void*, int32_t) { return {}; }
bool WriteInt32ArrayField(void*, int32_t, const std::vector<int32_t>&) { return true; }
}

namespace ue_wrap::log {
void Write(Level, const char*, ...) {}
}

int main() {
    using namespace ue_wrap::floppy_slot;
    Scalars state{};
    Content content{};

    ResetCache();
    test::provideZipProperty = true;
    test::zipMask = 0;
    CHECK(!EnsureResolved(DeviceKind::Laptop));
    CHECK(test::boolPropertyLookups == 1);
    CHECK(test::zipOffsetLookups == 0);

    ResetCache();
    test::zipMask = 0x04;
    CHECK(EnsureResolved(DeviceKind::Laptop));
    CHECK(test::zipOffsetLookups == 0);

    test::device.flags = 0xAB;
    CHECK(ReadScalars(DeviceKind::Laptop, &test::device, state));
    CHECK(!state.zip);

    state.zip = true;
    CHECK(WriteSlot(DeviceKind::Laptop, &test::device, state, content));
    CHECK(test::device.flags == 0xAF);
    CHECK(ReadScalars(DeviceKind::Laptop, &test::device, state) && state.zip);

    uint64_t originalDigest = 0;
    CHECK(ReadDigest(DeviceKind::Laptop, &test::device, originalDigest,
                     DigestScope::Occupancy));
    test::device.flags ^= 0x80;
    uint64_t neighborChangedDigest = 0;
    CHECK(ReadDigest(DeviceKind::Laptop, &test::device, neighborChangedDigest,
                     DigestScope::Occupancy));
    CHECK(originalDigest == neighborChangedDigest);

    state.zip = false;
    CHECK(WriteSlot(DeviceKind::Laptop, &test::device, state, content));
    CHECK(test::device.flags == 0x2B);
    uint64_t zipChangedDigest = 0;
    CHECK(ReadDigest(DeviceKind::Laptop, &test::device, zipChangedDigest,
                     DigestScope::Occupancy));
    CHECK(zipChangedDigest != neighborChangedDigest);

    ResetCache();
    test::provideZipProperty = false;
    CHECK(!EnsureResolved(DeviceKind::Laptop));
    CHECK(test::zipOffsetLookups == 0);
    puts("PASS floppy slot wrapper: bitfield read/write preserves neighbors; occupancy digest "
         "tracks zip only; missing/zero mask fails closed");
}
