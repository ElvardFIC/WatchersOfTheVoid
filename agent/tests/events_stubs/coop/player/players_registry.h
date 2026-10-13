#pragma once
#include <cstdint>
namespace coop::players {class Registry{public:static Registry& Get(){static Registry r;return r;}bool IsLocal(void*);uint8_t LocalPeerId()const;};}
