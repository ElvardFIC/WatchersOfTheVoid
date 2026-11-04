#pragma once
#define NowMs UnusedNativeNowMs
#include "../../../../../src/votv-coop/include/coop/props/active_drive.h"
#undef NowMs
namespace coop::active_drive {uint64_t NowMs();}
