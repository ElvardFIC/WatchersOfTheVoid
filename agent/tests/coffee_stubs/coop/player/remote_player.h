#pragma once
namespace coop::players {
class RemotePlayer {
public:
    void* actor = nullptr;
    bool posed = true;
    bool valid() const {return actor != nullptr;}
    bool HasPose() const {return posed;}
    void* GetActor() const {return actor;}
};
}
