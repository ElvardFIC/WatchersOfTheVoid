#pragma once
#include "ue_wrap/core/types.h"
namespace ue_wrap::event_location {
struct Box { FTransform transform{}; FVector extent{}; };
void* Controller(const wchar_t* field);
void* TriggerBox(void* controller);
bool Armed(void* box);
bool ReadBox(void* box,Box& bounds);
bool Contains(const Box& bounds,const FVector& point);
bool RunBox(void* box);
// Exact native agrav query's evaluated input locals (world-axis aligned box).
bool ReadAgravQuery(void* function,const uint8_t* locals,Box& bounds);
}
