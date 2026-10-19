#pragma once
#include <cstdint>
namespace coop::element {
using ElementId=uint32_t;
struct Element { void* actor=nullptr; void* LiveActor(){return actor;} };
}
