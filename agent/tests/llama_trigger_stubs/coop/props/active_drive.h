#pragma once
#include <cstdint>
namespace test {extern uint64_t now;}
namespace coop::active_drive {inline uint64_t NowMs(){return test::now;}}
