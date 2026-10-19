#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace ue_wrap::shared_screen {
struct Row {
    std::string path;
    uint8_t visibility = 0, flags = 0;
    int32_t tab = 0;
    float scroll = 0;
    std::wstring text;
};
bool Read(std::vector<Row>& rows);
bool Apply(const std::vector<Row>& rows);
}
