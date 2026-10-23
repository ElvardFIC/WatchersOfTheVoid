#pragma once
namespace coop::players {
class Registry {
public:
 void* local=nullptr;
 static Registry& Get(){static Registry registry;return registry;}
 void* Local(){return local;}
};
}
