#pragma once
namespace coop::players {
class Registry {
public:
    static Registry& Get(){static Registry registry;return registry;}
    void* Local(){return this;}
};
}
