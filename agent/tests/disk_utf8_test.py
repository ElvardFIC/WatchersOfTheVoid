"""Compile production disk encoders and check lossless Windows UTF-8 framing."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/disk-utf8-check'
out.mkdir(parents=True, exist_ok=True)


def function(src, signature):
    start = src.index(signature)
    end = src.index('{', start) + 1
    depth = 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end]


codec = root / 'src/votv-coop/src/coop/text/utf8_codec.cpp'
parts = ['#include <windows.h>\n#include <string>\n#include <vector>\n#include <cstdint>\n#include <cassert>',
         '#include "' + codec.as_posix() + '"',
         'namespace ue_wrap::log {void Write(Level,const char*,...) {}}',
         'using coop::text::ToUtf8Exact;\nstd::wstring FromUtf8(const std::string& s){std::wstring out;assert(coop::text::FromUtf8Strict(s.data(),s.size(),&out));return out;}']
for i, name in enumerate(('floppy_slot_sync', 'floppybox_sync', 'laptop_buffer_sync')):
    src = (root / f'src/votv-coop/src/coop/interactables/{name}.cpp').read_text()
    put, integer, width = ('PutStr32', 'PutU32', 4) if i == 0 else ('PutStr', 'PutU16', 2)
    parts.append(f'namespace lane{i} {{')
    for signature in ('void ' + integer + '(', 'void ' + put + '('):
        parts.append(function(src, signature))
    body = r'''
void Check() {
    for (const std::wstring w : {std::wstring(), std::wstring(L"$t\r\nServer data: test\n/n kilo\n/d abc\t"), std::wstring(L"\u041a\u043e\u0434 \u4e2d \xD83D\xDE00"), std::wstring(L"a\0b",3)}) {
        const auto encoded = ToUtf8Exact(w);
        assert(FromUtf8(encoded) == w);
        std::vector<uint8_t> wire; PUT(wire,w);
        unsigned size=0; for(unsigned n=0;n<WIDTH;++n) size|=unsigned(wire[n])<<(8*n);
        assert(size == encoded.size());
        assert(std::string(wire.begin()+WIDTH,wire.end()) == encoded);
        assert(FromUtf8(std::string(wire.begin()+WIDTH,wire.end())) == w);
    }
    assert(ToUtf8Exact(L"a\nb") != ToUtf8Exact(L"ab"));
    if constexpr (WIDTH == 2) {
        std::vector<uint8_t> wire;
        PUT(wire,std::wstring(40000,L'\u4e2d'));
        const unsigned size=wire[0]|(unsigned(wire[1])<<8);
        assert(size<=65535 && size==wire.size()-2);
        assert(FromUtf8(std::string(wire.begin()+2,wire.end()))==std::wstring(size/3,L'\u4e2d'));
    }
}
}
'''
    parts.append(body.replace('PUT', put).replace('WIDTH', str(width)))
parts.append('int main(){assert(coop::text::RunUtf8CodecSelftest());lane0::Check();lane1::Check();lane2::Check();}')
source = out / 'disk_utf8.cpp'
source.write_text('\n'.join(parts))
subprocess.run(['cl', '/nologo', '/std:c++20', '/EHsc', '/utf-8', '/I' + str(root / 'src/votv-coop/include'), str(source), '/Fo' + str(out / 'disk_utf8.obj'), '/Fe' + str(out / 'disk_utf8.exe')], check=True)
subprocess.run([str(out / 'disk_utf8.exe')], check=True)
print('PASS: three production encoders preserve CR/LF, tabs, Unicode, empty and embedded-NUL data through wire framing')
