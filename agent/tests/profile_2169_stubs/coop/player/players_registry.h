#pragma once
namespace coop::players {
inline void* local=nullptr;
class Registry {public:static Registry& Get(){static Registry r;return r;}void* Local(){return local;}};
}
