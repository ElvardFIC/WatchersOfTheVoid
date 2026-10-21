#pragma once
#include <cstdint>
#include <functional>
namespace ue_wrap::grill {
struct State {
    bool active=false,lid=false,cooking=false;
    int32_t coals=0;
    float burn=0,volume=0;
};
bool IsGrill(void* actor);
bool Valid(const State& state);
bool Read(void* actor,State& state);
struct Stock {int32_t coals=0;bool lid=false;};
bool ReadStock(void* actor,Stock& stock,const std::function<bool()>& current={});
enum class Edit {Retry,Applied,Interrupted};
// Changes exactly one native field; never replays item consumption or a lid factory.
Edit ChangeStock(void* actor,uint8_t operation,int32_t value,const std::function<bool()>& current);
// Native repaint and audio only; no fuel, heat, item or timeline producer.
bool Apply(void* actor,const State& state,const std::function<bool()>& current={});
bool StopSound(void* actor,const std::function<bool()>& current={});
}
