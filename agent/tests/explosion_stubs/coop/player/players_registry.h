#pragma once
namespace coop::players {
class Registry {
public:
 static Registry& Get(){static Registry r;return r;}
 bool IsLocal(void*);
};
}
